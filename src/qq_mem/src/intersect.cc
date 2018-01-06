#include "intersect.h"


TermScoreMap score_terms_in_doc(const TfIdfStore &tfidf_store, 
    TfIdfStore::row_iterator row_it,
    const qq_float &avg_doc_length, 
    const int &doc_length,
    const int &total_docs)
{
  TfIdfStore::col_iterator col_it; 
  TermScoreMap term_scores;

  for (col_it = TfIdfStore::col_cbegin(row_it);
       col_it != TfIdfStore::col_cend(row_it);
       col_it++) 
  {
    // each column contains a term.
    Term cur_term = TfIdfStore::GetCurTerm(col_it);
    int cur_term_freq = TfIdfStore::GetCurTermFreq(col_it);
    int doc_freq = tfidf_store.GetDocCount(cur_term);

    DLOG(INFO) << "cur_term: " << cur_term;
    DLOG(INFO) << "cur_term_freq: " << cur_term_freq;
    DLOG(INFO) << "doc_freq: " << doc_freq;

    qq_float idf = calc_es_idf(total_docs, doc_freq);
    qq_float tfnorm = calc_es_tfnorm(cur_term_freq, doc_length, avg_doc_length);

    // ES 6.1 uses tfnorm * idf 
    qq_float term_doc_score = idf * tfnorm;

    DLOG(INFO) << "idf: " << idf;
    DLOG(INFO) << "tfnorm: " << tfnorm;
    DLOG(INFO) << "term_doc_score: " << term_doc_score;

    term_scores[cur_term] = term_doc_score;
  }

  return term_scores;
}


TermScoreMap score_terms_in_doc(const IntersectionResult &intersection_result, 
    IntersectionResult::row_iterator row_it,
    const qq_float &avg_doc_length, 
    const int &doc_length,
    const int &total_docs)
{
  IntersectionResult::col_iterator col_it; 
  TermScoreMap term_scores;

  for (col_it = IntersectionResult::col_cbegin(row_it);
       col_it != IntersectionResult::col_cend(row_it);
       col_it++) 
  {
    // each column contains a term.
    Term cur_term = IntersectionResult::GetCurTerm(col_it);
    const RankingPosting *posting = IntersectionResult::GetPosting(col_it);
    int cur_term_freq = posting->GetTermFreq();
    int doc_freq = intersection_result.GetDocCount(cur_term);

    DLOG(INFO) << "cur_term: " << cur_term;
    DLOG(INFO) << "cur_term_freq: " << cur_term_freq;
    DLOG(INFO) << "doc_freq: " << doc_freq;

    qq_float idf = calc_es_idf(total_docs, doc_freq);
    qq_float tfnorm = calc_es_tfnorm(cur_term_freq, doc_length, avg_doc_length);

    // ES 6.1 uses tfnorm * idf 
    qq_float term_doc_score = idf * tfnorm;

    DLOG(INFO) << "idf: " << idf;
    DLOG(INFO) << "tfnorm: " << tfnorm;
    DLOG(INFO) << "term_doc_score: " << term_doc_score;

    term_scores[cur_term] = term_doc_score;
  }

  return term_scores;
}







qq_float aggregate_term_score(const TermScoreMap &term_scores) {
  qq_float doc_score = 0;
  for (TermScoreMap::const_iterator it = term_scores.cbegin();
       it != term_scores.cend(); it++) 
  {
    doc_score += it->second; 
  }
  return doc_score;
}

DocScoreVec score_docs(const TfIdfStore &tfidf_store, const DocLengthStore &doc_lengths) {
  TfIdfStore::row_iterator row_it; 
  TfIdfStore::col_iterator col_it; 

  qq_float avg_doc_length = doc_lengths.GetAvgLength();
  int total_docs = doc_lengths.Size();

  DLOG(INFO) << "--------------------- New Scoring -----------------------";
  DLOG(INFO) << doc_lengths.ToStr();
  DLOG(INFO) << "avg_doc_length: " << avg_doc_length;
  DLOG(INFO) << "total_docs: " << total_docs;

  DocScoreVec doc_scores;
  for (row_it = tfidf_store.row_cbegin(); 
       row_it != tfidf_store.row_cend(); 
       row_it++) 
  {
    // each row contains a document.
    DocIdType cur_doc_id = TfIdfStore::GetCurDocId(row_it);
    int doc_length = doc_lengths.GetLength(cur_doc_id);

    DLOG(INFO) << "cur_doc_id: " << cur_doc_id;
    DLOG(INFO) << "doc_length: " << doc_length;

    TermScoreMap term_scores = score_terms_in_doc(tfidf_store, row_it, 
        avg_doc_length, doc_length, total_docs);

    // doc_scores[cur_doc_id] = aggregate_term_score(term_scores);
    doc_scores.emplace_back(cur_doc_id, aggregate_term_score(term_scores));
  }

  return doc_scores;
}

DocScoreVec score_docs(const IntersectionResult &intersection_result, 
                       const DocLengthStore &doc_lengths) {
  IntersectionResult::row_iterator row_it; 
  IntersectionResult::col_iterator col_it; 

  qq_float avg_doc_length = doc_lengths.GetAvgLength();
  int total_docs = doc_lengths.Size();

  DLOG(INFO) << "--------------------- New Scoring -----------------------";
  DLOG(INFO) << doc_lengths.ToStr();
  DLOG(INFO) << "avg_doc_length: " << avg_doc_length;
  DLOG(INFO) << "total_docs: " << total_docs;

  DocScoreVec doc_scores;
  for (row_it = intersection_result.row_cbegin(); 
       row_it != intersection_result.row_cend(); 
       row_it++) 
  {
    // each row contains a document.
    DocIdType cur_doc_id = IntersectionResult::GetCurDocId(row_it);
    int doc_length = doc_lengths.GetLength(cur_doc_id);

    DLOG(INFO) << "cur_doc_id: " << cur_doc_id;
    DLOG(INFO) << "doc_length: " << doc_length;

    TermScoreMap term_scores = score_terms_in_doc(intersection_result, row_it, 
        avg_doc_length, doc_length, total_docs);

    doc_scores.emplace_back(cur_doc_id, aggregate_term_score(term_scores));
  }

  return doc_scores;
}







