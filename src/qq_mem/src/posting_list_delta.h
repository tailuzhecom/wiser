#ifndef POSTING_LIST_DELTA_H
#define POSTING_LIST_DELTA_H

#include <glog/logging.h>

#include "compression.h"


class OffsetPairsIterator {
 public:
  OffsetPairsIterator(const VarintBuffer *data, int byte_offset, 
      const int end_offset)
    :data_(data), byte_offset_(byte_offset), end_offset_(end_offset),
     pair_index_(0) {}

  bool Advance(OffsetPair *pair) {
    if (byte_offset_ == end_offset_) {
      return false;
    }
    
    uint32_t n;
    int len;

    const std::string *data = data_->DataPointer();

    len = utils::varint_decode(*data, byte_offset_, &n);
    byte_offset_ += len;
    std::get<0>(*pair) = n;

    len = utils::varint_decode(*data, byte_offset_, &n);
    byte_offset_ += len;
    std::get<1>(*pair) = n;

    return true;
  }

 private:
  const VarintBuffer *data_;
  int byte_offset_; 
  const int end_offset_; // the start offset of the next posting
  int pair_index_;
};


struct SpanMeta {
  DocIdType prev_doc_id; // doc ID right before this span
  int start_offset;

  SpanMeta(DocIdType id, int offset)
    : prev_doc_id(id), start_offset(offset) {}
};

// typedef std::vector<DocIdType> SkipIndex;
typedef std::vector<SpanMeta> SkipIndex;

class PostingListDeltaIterator {
 public:
  PostingListDeltaIterator(const VarintBuffer *data, 
                           const SkipIndex *skip_index,
                           const int total_postings,
                           DocIdType prev_doc_id, 
                           int byte_offset)
    :data_(data), 
     skip_index_(skip_index),
     total_postings_(total_postings),
     cur_posting_index_(0),
     prev_doc_id_(prev_doc_id), 
     byte_offset_(byte_offset)
  {
    DecodeToCache();
  }

  bool Advance() {
    if (IsEnd()) {
      return false;
    }

    byte_offset_ = cache_.next_posting_byte_offset_; 
    cur_posting_index_++;

    DecodeToCache();
    return true;
  }

  bool HasSkip() const {
    // return cur_posting_index_ % skip_span_ == 0 && 
      // cur_posting_index_ + skip_span_ < total_postings_;
  }

  DocIdType SkipDocId() const {
    // assert(HasSkip());
    // int span_index = cur_posting_index_ / skip_span_;  
    
  }

  void SkipForward(DocIdType doc_id) {
    // Move the iterator to a posting that has a doc id that is 
    // larger or equal to doc_id
    // It moves to the end if it cannout find such posting
    // TODO: Advance() with less cost

    // loop inv: 
    //   posting[0, cur_posting_index_).doc_id < doc_id
    //   byte_offset_ is the offset of posting[cur_posting_index_]
    //   prev_doc_id_ is the doc id of posting[cur_posting_index_ - 1]
    //   cache_ has the data of posting[cur_posting_index_]

    // while (cur_posting_index_ < total_postings && cache_.cur_doc_id_ < doc_id) {
      // if (HasSkip() && 
    // }

  }

  bool IsEnd() {
    return cur_posting_index_ == total_postings_;
  }

  DocIdType DocId() {
    return cache_.cur_doc_id_;
  }

  int TermFreq() {
    return cache_.cur_term_freq_;
  }

  int OffsetPairStart() {
    return cache_.cur_offset_pairs_start_;
  }

  OffsetPairsIterator OffsetPairsBegin() {
    if (IsEnd()) {
      LOG(FATAL) 
        << "Trying to get offset iterator from a empty posting iterator\n";
    }
    return OffsetPairsIterator(data_, 
                               cache_.cur_offset_pairs_start_,
                               cache_.next_posting_byte_offset_);
  }

  int CurContentBytes() {
    return cache_.cur_content_bytes_;
  }
  
 private:
  void DecodeToCache() {
    int offset = byte_offset_;
    uint32_t delta;
    int len;

    const std::string *data = data_->DataPointer();

    len = utils::varint_decode(*data, offset, &cache_.cur_content_bytes_);
    offset += len;
    cache_.next_posting_byte_offset_ = offset + cache_.cur_content_bytes_;

    len = utils::varint_decode(*data, offset, &delta);
    offset += len;
    len = utils::varint_decode(*data, offset, &cache_.cur_term_freq_);
    offset += len;

    cache_.cur_doc_id_ = prev_doc_id_ + delta;
    cache_.cur_offset_pairs_start_ = offset; 
  }

  const VarintBuffer * data_;
  const SkipIndex *skip_index_;
  int byte_offset_; // start byte of the posting
  int cur_posting_index_;
  DocIdType prev_doc_id_;
  const int total_postings_;

  // cache
  struct PostingCache {
    int next_posting_byte_offset_;
    DocIdType cur_doc_id_;
    uint32_t cur_term_freq_;
    int cur_offset_pairs_start_;
    uint32_t cur_content_bytes_;
  };
  PostingCache cache_;
};


class PostingListDelta {
 public:
  PostingListDelta(Term term) :term_(term), skip_span_(100) {}
  PostingListDelta(Term term, const int skip_span) 
    :term_(term), skip_span_(skip_span) {}

  // Assume posting[-1] = posting[0], so delta[0] is always 0
  void AddPosting(const RankingPostingWithOffsets &posting) {
    DocIdType doc_id = posting.GetDocId();

    if (posting_idx_ == 0) {
      // initialize for the first posting
      last_doc_id_ = doc_id;
    }

    if (posting_idx_ % skip_span_) {
      skip_index_.push_back(SpanMeta(last_doc_id_, data_.End()));
    }

    DocIdType delta = doc_id - last_doc_id_;
    RankingPostingWithOffsets posting_with_delta(delta, 
                                                 posting.GetTermFreq(), 
                                                 *posting.GetOffsetPairs());
    data_.Append(posting_with_delta.Encode());

    last_doc_id_ = doc_id;
    posting_idx_++;
  }

  PostingListDeltaIterator Begin() {
    if (posting_idx_ == 0) {
      LOG(FATAL) << "Posting List must have at least one posting" << std::endl;
    }
    return PostingListDeltaIterator(&data_, &skip_index_, Size(), 
        skip_index_[0].prev_doc_id, 0);
  }

  int Size() {
    return posting_idx_;
  }

  int ByteCount() {
    return data_.Size();
  }

 private:
  VarintBuffer data_;
  const Term term_;
  int posting_idx_ = 0; 
  DocIdType last_doc_id_;
  // [0]: doc id of posting[-1]
  // [1]: doc id of posting[skip_span_ - 1] 
  // [2]: doc id of posting[skip_span_ * 2 - 1] 
  // ...
  SkipIndex skip_index_;
  const int skip_span_;
};

#endif