#include "Posting_List.h"

// Posting:
    Posting::Posting(int docID_in, int term_frequency_in, int position_in[]) {
        docID = docID_in;
        term_frequency = term_frequency_in;
        position = (int *) malloc(sizeof(int)*term_frequency);
        memcpy(position, position_in, sizeof(int)*term_frequency); 
        next = NULL;
    }

    std::string Posting::dump() {
        std::string result="-";
        result = result + std::to_string(docID)+ "_" + std::to_string(term_frequency);
        for (int i = 0; i < term_frequency; i++) {
            result += "_" + std::to_string(position[i]);
        }
        return result;
    }



// Posting_List:
// Init with a term string, when creating index
    Posting_List::Posting_List(std::string term_in) {  // do we need term?
        term = term_in;
        num_postings = 0;
        p_list = NULL;
    }
// Init with a term string, and posting list string reference, when query processing
    Posting_List::Posting_List(std::string term_in, std::string & serialized_in) {  // configuration
        term = term_in;
        serialized = serialized_in;
        cur_index = -1;
        cur_docID = -1;
        // parse string and read posting list head(num_postings)
        plist_message.ParseFromString(serialized_in);
        num_postings = plist_message.num_postings();
        // hint
        //std::cout<< "num of postings: " << num_postings << std::endl;
    }

// Get next posting
    Posting * Posting_List::next() {  // exactly next
        return next(cur_docID+1);
    }

    Posting * Posting_List::next(int next_doc_ID) { // next Posting whose docID>next_doc_ID
        // get the string for next Posting
        while (cur_docID < next_doc_ID) {
            get_next_index();
            if (cur_index == num_postings)  // get the end
                return NULL;
            get_cur_DocID();
        }
        return get_next_Posting();
    }

    void Posting_List::get_next_index() {  // get to next index which is the starting point of a Posting
        if (cur_index == num_postings)
            return;
        cur_index++;
    }

    void Posting_List::get_cur_DocID() {  // get the DocID of current Posting which starts from cur_index
        cur_docID = plist_message.postings(cur_index).docid();
    }

    Posting * Posting_List:: get_next_Posting() {  // read and parse the Posting which starts from cur_index, after this: cur_index is the end of this posting
        //printf("\nget here before next_posting: cur index:%d cur_DocID: %d", cur_index, cur_docID);
        // parse positions
        
        posting_message::Posting cur_posting = plist_message.postings(cur_index);
        int tmp_frequency = cur_posting.term_frequency();
        int * tmp_positions = (int *) malloc(tmp_frequency * sizeof(int));
        for (int i = 0; i < tmp_frequency; i++)
           tmp_positions[i] = cur_posting.positions(i);
        return new Posting(cur_posting.docid(), tmp_frequency, tmp_positions);
    }

// Add a doc for creating index
    void Posting_List::add_doc_new(std::string & doc_string) {  // use protobuf
        printf("add new doc\n");

        // parse string
        posting_message::doc_info this_doc;
        this_doc.ParseFromString(doc_string);

        int tmp_ID, tmp_freq;
        tmp_ID = this_doc.id();
        tmp_freq = this_doc.term_frequency();
        // add one posting to the p_list
        std::cout << "New Add document " << tmp_ID << " " << tmp_freq << " for " << term << std::endl;
        for (int i = 0; i < this_doc.positions_size(); i++)
            printf(" %d", this_doc.positions(i));
        printf("\n"); 
       
        return; 

    }

    void Posting_List::add_doc(int docID, int term_frequency, int position[]) { // TODO what info? who should provide?
        std::cout << "Add document " << docID << " for " << term << std::endl;
        // add one posting to the p_list
        num_postings += 1;
        if (p_list == NULL) {
            p_list = new Posting(docID, term_frequency, position);
        } else {  // insert into the posting list sorted by docID
            Posting * tmp_posting = p_list;
            Posting * tmp1_posting = NULL;
            while (tmp_posting != NULL) {
                if (tmp_posting->docID > docID)
                    break;
                tmp1_posting = tmp_posting;
                tmp_posting = tmp_posting->next;
            }
            Posting * tmp2_posting = new Posting(docID, term_frequency, position);
            tmp2_posting->next = tmp_posting;
            if (tmp1_posting != NULL) {
                tmp1_posting->next = tmp2_posting; 
            } else {
                p_list = tmp2_posting;
            }
        }

        return;
    }

// Serialize the posting list
    std::string Posting_List::serialize() {
        /* format: protobuf message in posting_message.proto
        */
        posting_message::PostingList pl_message;
        {
            // header: num_postings 
            pl_message.set_num_postings(num_postings);
            // postings:
            Posting * cur_posting = p_list;
            while (p_list != NULL) {
                posting_message::Posting * new_posting = pl_message.add_postings();
                new_posting->set_docid(p_list->docID);
                new_posting->set_term_frequency(p_list->term_frequency);
                for (int k = 0; k < p_list->term_frequency; k++) 
                    new_posting->add_positions(p_list->position[k]);
                p_list = p_list->next;
            }
        }
        std::string pl_string;
        pl_message.SerializeToString(&pl_string);
        return pl_string;
    }
