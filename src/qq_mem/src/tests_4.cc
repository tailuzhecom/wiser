#include "catch.hpp"

#include "utils.h"
#include "compression.h"
#include "posting.h"
#include "posting_list_delta.h"



void test_encoding(uint32_t val) {
  std::string buf(8, '\0');
  uint32_t val2;

  auto len1 = utils::varint_encode(val, &buf, 0);
  auto len2 = utils::varint_decode(buf, 0, &val2);
  
  REQUIRE(val == val2);
  REQUIRE(len1 == len2);
  REQUIRE(len1 > 0);
}


void test_expand_and_encode(uint32_t val) {
  std::string buf;
  uint32_t val2;

  auto len1 = utils::varint_expand_and_encode(val, &buf, 0);
  auto len2 = utils::varint_decode(buf, 0, &val2);
  
  REQUIRE(val == val2);
  REQUIRE(len1 == len2);
  REQUIRE(len1 > 0);
}


TEST_CASE( "varint", "[varint]" ) {
  test_encoding(0);
  test_encoding(10);
  test_encoding(300);
  test_encoding(~0x00); // largest number possible

  test_expand_and_encode(0);
  test_expand_and_encode(10);
  test_expand_and_encode(300);
  test_expand_and_encode(~0x00); // largest number possible

  std::string buf(8, '\0');
  uint32_t val02;

  SECTION("Protobuf example") {
    auto len1 = utils::varint_encode(300, &buf, 0);
    REQUIRE(len1 == 2);
    // printf("----------=\n");
    // printf("%04x \n", buf[0] & 0xff);
    // printf("%04x \n", buf[1] & 0xff);
    REQUIRE((buf[0] & 0xff) == 0xAC);
    REQUIRE((buf[1] & 0xff) == 0x02);

    auto len2 = utils::varint_decode(buf, 0, &val02);
    REQUIRE(len2 == 2);
    REQUIRE(val02 == 300);
  }

  SECTION("Protobuf example") {
    auto len1 = utils::varint_encode(0, &buf, 0);
    REQUIRE(len1 == 1);
    // printf("----------=\n");
    // printf("%04x \n", buf[0] & 0xff);
    // printf("%04x \n", buf[1] & 0xff);
    REQUIRE((buf[0] & 0xff) == 0x00);

    auto len2 = utils::varint_decode(buf, 0, &val02);
    REQUIRE(len2 == 1);
    REQUIRE(val02 == 0);
  }
}

TEST_CASE( "VarintBuffer", "[varint]" ) {
  SECTION("Append and prepend number") {
    VarintBuffer buf;
    REQUIRE(buf.End() == 0);

    buf.Append(1);
    REQUIRE(buf.End() == 1);

    auto data = buf.Data();
    REQUIRE(data[0] == 1);

    buf.Prepend(2);
    REQUIRE(buf.End() == 2);

    data = buf.Data();
    REQUIRE(data[0] == 2);

    REQUIRE(data.size() == 2);
  }

  SECTION("Append and prepend string buffer") {
    VarintBuffer buf;
    buf.Append("abc");
    buf.Append("def");
    REQUIRE(buf.Size() == 6);
    REQUIRE(buf.Data() == "abcdef");

    buf.Prepend("01");
    REQUIRE(buf.Size() == 8);
    REQUIRE(buf.Data() == "01abcdef");
  }
}


TEST_CASE( "Encoding posting", "[encoding]" ) {
  SECTION("No offsets") {
    RankingPostingWithOffsets posting(3, 4); 
    std::string buf = posting.Encode();
    REQUIRE(buf[0] == 2); // num of bytes starting from doc id
    REQUIRE(buf[1] == 3); // doc id
    REQUIRE(buf[2] == 4); //  TF
  }

  SECTION("With offset pairs") {
    OffsetPairs offset_pairs;
    for (int i = 0; i < 10; i++) {
      offset_pairs.push_back(std::make_tuple(1, 2)); 
    }

    RankingPostingWithOffsets posting(3, 4, offset_pairs); 
    std::string buf = posting.Encode();
    REQUIRE(buf[0] == 22); // content size: 2 + 2 * 10 = 22
    REQUIRE(buf[1] == 3); // doc id
    REQUIRE(buf[2] == 4); //  TF

    const auto PRE = 3; // size | doc_id | TF | 
    for (int i = 0; i < 10; i++) {
      REQUIRE(buf[PRE + 2 * i] == 1);
      REQUIRE(buf[PRE + 2 * i + 1] == 2);
    }
  }
}

void test_offset_iterator(OffsetPairsIterator offset_it, 
    OffsetPairs original_pairs) {
  OffsetPairs pairs(original_pairs.size());
  for (int i = 0; i < original_pairs.size(); i++) {
    auto ret = offset_it.Advance(&pairs[i]);
    REQUIRE(ret == true);
    REQUIRE(std::get<0>(pairs[i]) == std::get<0>(original_pairs[i]));
    REQUIRE(std::get<1>(pairs[i]) == std::get<1>(original_pairs[i]));
  }

  // check advancing last one
  {
    OffsetPair tmp_pair;
    auto ret = offset_it.Advance(&tmp_pair);
    REQUIRE(ret == false);
  }
}

void test_posting_list_delta( RankingPostingWithOffsets postingA,
    RankingPostingWithOffsets postingB) {
  // Initialize posting list
  PostingListDelta pl("hello");
  REQUIRE(pl.Size() == 0);

  pl.AddPosting(postingA);

  REQUIRE(pl.Size() == 1);
  REQUIRE(pl.ByteCount() == postingA.Encode().size());

  pl.AddPosting(postingB);

  REQUIRE(pl.Size() == 2);
  REQUIRE(pl.ByteCount() == postingA.Encode().size() + postingB.Encode().size() );

  // Iterate
  auto it = pl.Begin();
  REQUIRE(it.IsEnd() == false);
  REQUIRE(it.DocId() == postingA.GetDocId());
  REQUIRE(it.TermFreq() == postingA.GetTermFreq());
  REQUIRE(it.OffsetPairStart() == 3); // size|id|tf|offset
  REQUIRE(it.CurContentBytes() == postingA.Encode().size() - 1); // size|id|tf|offset

  // check offsets
  {
    auto offset_it = it.OffsetPairsBegin();
    auto original_pairs = *postingA.GetOffsetPairs();
    test_offset_iterator(offset_it, original_pairs);
  }

  // Iterate
  it.Advance();
  REQUIRE(it.IsEnd() == false);
  REQUIRE(it.DocId() == postingB.GetDocId());
  REQUIRE(it.TermFreq() == postingB.GetTermFreq());
  REQUIRE(it.OffsetPairStart() == postingA.Encode().size() + 3); // size|id|tf|offset
  REQUIRE(it.CurContentBytes() == postingB.Encode().size() - 1); // size|id|tf|offset

  // check offsets
  {
    auto offset_it = it.OffsetPairsBegin();
    auto original_pairs = *postingB.GetOffsetPairs();
    test_offset_iterator(offset_it, original_pairs);
  }

  it.Advance();
  REQUIRE(it.IsEnd() == true);
}

TEST_CASE( "Posting List Delta", "[postinglist]" ) {
  SECTION("10 postings") {
    OffsetPairs offset_pairsA;
    for (int i = 0; i < 10; i++) {
      offset_pairsA.push_back(std::make_tuple(i * 2, i * 2 + 1)); 
    }

    OffsetPairs offset_pairsB;
    for (int i = 20; i < 23; i++) {
      offset_pairsB.push_back(std::make_tuple(i * 2, i * 2 + 1)); 
    }

    RankingPostingWithOffsets postingA(3, 4, offset_pairsA); 
    RankingPostingWithOffsets postingB(8, 9, offset_pairsB); 

    test_posting_list_delta(postingA, postingB);
  }

  SECTION("0 postings") {
    OffsetPairs offset_pairs;

    RankingPostingWithOffsets postingA(3, 4, offset_pairs); 
    RankingPostingWithOffsets postingB(8, 9, offset_pairs); 

    test_posting_list_delta(postingA, postingB);
  }
}


