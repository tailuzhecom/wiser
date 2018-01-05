#ifndef POSTING_BASIC_H
#define POSTING_BASIC_H

#include "engine_services.h"
#include "types.h"
#include <unordered_map>
#include <cereal/types/vector.hpp>
#include <cereal/types/tuple.hpp>
#include <cereal/types/utility.hpp>
#include <cereal/types/unordered_map.hpp>
#include <cereal/archives/binary.hpp>
#include <sstream>

// TODO will delete them
typedef int Position;
typedef std::vector<Position> Positions;


class Posting : public PostingService {
    public:
        int docID_ = 0;
        int term_frequency_ = 0;
        Offsets positions_ = {};
        
        Passage_Scores passage_scores_ = {};
        std::unordered_map<int, Passage_Split> passage_splits_; // passage_id to passage_split
 
        Posting();
        Posting(const int & docID, const int & term_frequency, const Offsets & offsets_in);
        Posting(const int & docID, const int & term_frequency, 
                Offsets * offsets_in, Passage_Scores * passage_scores, std::unordered_map<int, Passage_Split> * passage_splits);
        std::string dump();

        // This method lets cereal know which data members to serialize
        template<class Archive>
        void serialize(Archive & archive)
        {
            archive( docID_, term_frequency_, positions_, passage_scores_, passage_splits_); // serialize things by passing them to the archive
        }
        // TODO Posting * next;
};


class PostingSimple : public PostingService {
 public:
  int docID_;
  int term_frequency_;
  Positions positions_;

  PostingSimple() :docID_(0), term_frequency_(0) {}
  PostingSimple(int docID, int term_frequency, const Positions positions)
        :docID_(docID), term_frequency_(term_frequency), positions_(positions)
  {}

  const int GetDocId() const {return docID_;}
  const int GetTermFreq() const {return term_frequency_;}
  const Positions GetPositions() const {return positions_;}
  std::string dump() {return "";}
};


class RankingPosting {
  public:
    int doc_id_;
    int term_frequency_;

    RankingPosting(){};
    RankingPosting(int doc_id, int term_frequency)
      :doc_id_(doc_id), term_frequency_(term_frequency)
    {
    }
    const int GetDocId() const {return doc_id_;}
    const int GetTermFreq() const {return term_frequency_;}
};


#endif