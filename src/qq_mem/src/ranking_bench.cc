#include <iostream>
#include <cassert>
#include <unordered_map>

#include <glog/logging.h>

#include "intersect.h"
#include "ranking.h"
#include "utils.h"
#include "posting_list_vec.h"
#include "qq_mem_uncompressed_engine.h"


typedef std::vector<int> TopDocs;
typedef std::tuple<int, int> Offset;  //startoffset, endoffset
typedef std::vector<Offset> Offsets;

class TermWithOffset {
	public:
		Term term_;
		Offsets offsets_;

		TermWithOffset(Term term_in, Offsets offsets_in) 
			: term_(term_in), offsets_(offsets_in) {} 

    const int CalcTermFreq() const {
      return offsets_.size();  
    }
};

typedef std::vector<TermWithOffset> TermWithOffsetList;

class FieldLengthStore {
 public:
  std::unordered_map<DocIdType, int> store_;

  void SetLength(DocIdType doc_id, int len) {
    store_[doc_id] = len;
  }

  int GetLength(DocIdType doc_id) {
    return store_[doc_id];
  }
};



// Parse offsets from string
void handle_positions(const std::string& s, Offset& this_position) {
    std::size_t pos_split = s.find(",");
    this_position = std::make_tuple(std::stoi(s.substr(0, pos_split)), std::stoi(s.substr(pos_split+1)));
    // construct position
    return;
}


void handle_term_offsets(const std::string& s, Offsets& this_term) {
    std::string buff{""};

    for(auto n:s) {
        // split by ;
        if(n != ';') buff+=n; else
        if(n == ';' && buff != "") {
            Offset this_position;
            // split by ,
            handle_positions(buff, this_position);
            this_term.push_back(this_position);
            buff = "";
        }
    }

    return;
}

const std::vector<Offsets> parse_offsets(const std::string& s) {
    std::vector<Offsets> res;
    
    std::string buff{""};

    for(auto n:s)
    {
    // split by .
        if(n != '.') buff+=n; else
        if(n == '.' && buff != "") {
            Offsets this_term;
            handle_term_offsets(buff, this_term);
            res.push_back(this_term);
            buff = "";
        }
    }
    return res;
}

// return:
//   term1: offsets
//   term2: offsets
//   ...
TermWithOffsetList parse_doc(const std::vector<std::string> &items) {
	auto title = items[0];
	auto body = items[1];
	auto tokens = items[2];
	auto offsets = items[3];

	std::vector<Offsets> offsets_parsed = parse_offsets(offsets);
	std::vector<std::string> terms = utils::explode(tokens, ' ');
	TermWithOffsetList terms_with_offset = {};

	for (int i = 0; i < terms.size(); i++) {
		Offsets tmp_offsets = offsets_parsed[i];
		TermWithOffset cur_term(terms[i], tmp_offsets);
		terms_with_offset.push_back(cur_term);
	}

	return terms_with_offset;
}


int calc_total_terms(const TermWithOffsetList &termlist) {
  int total = 0;
  for (const auto &term_with_offset : termlist) {
    total += term_with_offset.CalcTermFreq(); 
  }
  return total;
}

void print_counts(CountMapType counts) {
  for (auto it = counts.begin(); it != counts.end(); it++) {
    std::cout << it->first << ":" << it->second << std::endl;
  }
}



template <class T>
void print_vec(T vec) {
  for (auto e : vec) {
    std::cout << e << "|";
  }
  std::cout << std::endl;
}

void test() {
  {
    InvertedIndexQqMem inverted_index;
    utils::LineDoc linedoc("./src/testdata/tiny-line-doc");

    for (int i = 0; i < 2; i++) {
      std::vector<std::string> items;
      linedoc.GetRow(items);
      inverted_index.AddDocument(i, items[0], items[1]);
    }
    // inverted_index.Search(TermList{"you"}, SearchOperator::AND);
    // auto result = inverted_index.SearchAndRank(TermList{"have"}, SearchOperator::AND);
  }
}


int main(int argc, char** argv) {
  google::InitGoogleLogging(argv[0]);
  // FLAGS_logtostderr = 1; // print to stderr instead of file
  FLAGS_stderrthreshold = 0; 
  FLAGS_minloglevel = 0; 

  test();

  InvertedIndexQqMem inverted_index;
  // This could be put to doc store in the future
  FieldLengthStore field_lengths; 

  utils::LineDoc linedoc("./src/testdata/tiny-line-doc");

  for (int i = 0; i < 2; i++) {
    // Process a document
    std::cout << i << std::endl;
    std::vector<std::string> items;
    linedoc.GetRow(items);

    std::cout << "body: " << items[0] << std::endl;
    std::cout << "tokens: " << items[1] << std::endl;

    inverted_index.AddDocument(i, items[0], items[1]);
    field_lengths.SetLength(i, count_terms(items[1]));
	}
}

