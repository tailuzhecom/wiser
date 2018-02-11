#include <algorithm>
#include <iostream>
#include <string>
#include <memory>
#include <thread>
#include <vector>
#include <climits>

#define NDEBUG
#include <glog/logging.h>

#include "qq_mem_engine.h"
#include "utils.h"
#include "general_config.h"
#include "query_pool.h"

const int K = 1000;
const int M = 1000 * K;
const int B = 1000 * M;



void load_query_pool(TermPool *pool, const GeneralConfig &config) {
  auto query_source_ = config.GetString("query_source");

  // config according to different mode
  if (query_source_ == "hardcoded") {
    pool->Add(config.GetStringVec("terms"));
  } else if (query_source_ == "querylog") {
    // read in all querys
    pool->LoadFromFile(config.GetString("querylog_path"), 0);
  } else {
    LOG(WARNING) << "Cannot determine query source";
  }
}



std::unique_ptr<SearchEngineServiceNew> create_engine(const int &step_height, 
    const int &step_width, const int &n_steps) {
  std::unique_ptr<SearchEngineServiceNew> engine = CreateSearchEngine("qq_mem_uncompressed");

  utils::Staircase staircase(step_height, step_width, n_steps);

  std::string doc;
  int cnt = 0;
  while ( (doc = staircase.NextLayer()) != "" ) {
    // std::cout << doc << std::endl;
    engine->AddDocument(DocInfo(doc, doc, "", "", "TOKEN_ONLY"));
    cnt++;

    if (cnt % 1000 == 0) {
      std::cout << cnt << std::endl;
    }
  }

  return engine;
}


std::unique_ptr<SearchEngineServiceNew> create_engine_from_file(
    const GeneralConfig &config) {
  std::unique_ptr<SearchEngineServiceNew> engine = CreateSearchEngine(config.GetString("engine_type"));
  engine->LoadLocalDocuments(config.GetString("linedoc_path"), 
                             config.GetInt("n_docs"),
                             config.GetString("loader"));
  std::cout << "Term Count: " << engine->TermCount() << std::endl;

  std::cout << "Sleeping 100000 sec..." << std::endl;
  utils::sleep(100000);

  return engine;
}

void show_engine_stats(SearchEngineServiceNew *engine, const TermList &terms) {
  auto pl_sizes = engine->PostinglistSizes(terms); 
  for (auto pair : pl_sizes) {
    std::cout << pair.first << " : " << pair.second << std::endl;
  }
}

utils::ResultRow search(SearchEngineServiceNew *engine, 
                        const GeneralConfig &config) {
  const int n_repeats = config.GetInt("n_repeats");
  utils::ResultRow row;
  
  // construct query pool
  TermPool query_pool;
  load_query_pool(&query_pool, config);
  std::cout << "Constructed query pool successfully" << std::endl;

  if (config.GetString("query_source") == "hardcoded") {
		std::cout << "Posting list sizes:" << std::endl;
    show_engine_stats(engine, config.GetStringVec("terms"));
  }

  auto enable_snippets = config.GetBool("enable_snippets");
  auto start = utils::now();
  for (int i = 0; i < n_repeats; i++) {
    // auto terms = query_pool.Next();
    // for (auto term: terms) {
      // std::cout << term << " ";
    // }
    // auto query = SearchQuery(terms, enable_snippets);

    auto query = SearchQuery(query_pool.Next(), enable_snippets);
    query.n_snippet_passages = config.GetInt("n_passages");
    auto result = engine->Search(query);

    // std::cout << result.ToStr() << std::endl;
  }
  auto end = utils::now();
  auto dur = utils::duration(start, end);

  row["duration"] = std::to_string(dur / n_repeats); 
  row["QPS"] = std::to_string(n_repeats / dur);
  return row;
}


void qq_uncompressed_bench() {
  utils::ResultTable table;
  auto engine = create_engine(1, 2, 425);

  // table.Append(search(engine.get(), TermList{"0"}));

  // table.Append(search(engine.get(), TermList{"0", "1"}));

  std::cout << table.ToStr();
}


void qq_uncompressed_bench_wiki(const GeneralConfig &config) {
  utils::ResultTable table;

  auto engine = create_engine_from_file(config);

  auto row = search(engine.get(), config);

  row["n_docs"] = std::to_string(config.GetInt("n_docs"));
  row["query_source"] = config.GetString("query_source");
  if (row["query_source"] == "hardcoded") {
    auto terms = config.GetStringVec("terms");
    for (auto term : terms) {
      row["query"] += term;
    }
  }
  table.Append(row);

  std::cout << table.ToStr();
}

GeneralConfig config_by_jun() {
/*
                                       /;    ;\
                                   __  \\____//
                                  /{_\_/   `'\____
                                  \___   (o)  (o  }
       _____________________________/          :--'   DRINKA
   ,-,'`@@@@@@@@       @@@@@@         \_    `__\
  ;:(  @@@@@@@@@        @@@             \___(o'o)
  :: )  @@@@          @@@@@@        ,'@@(  `===='        PINTA
  :: : @@@@@:          @@@@         `@@@:
  :: \  @@@@@:       @@@@@@@)    (  '@@@'
  ;; /\      /`,    @@@@@@@@@\   :@@@@@)                   MILKA
  ::/  )    {_----------------:  :~`,~~;
 ;;'`; :   )                  :  / `; ;
;;;; : :   ;                  :  ;  ; :                        DAY !!!
`'`' / :  :                   :  :  : :
    )_ \__;      ";"          :_ ;  \_\       `,','
    :__\  \    * `,'*         \  \  :  \   *  8`;'*  *
        `^'     \ :/           `^'  `-^-'   \v/ :  \/   -Bill Ames-

*/

  GeneralConfig config;
  config.SetString("engine_type", "qq_mem_compressed");
  // config.SetString("engine_type", "qq_mem_uncompressed");

  config.SetInt("n_docs", 10000000);

  // config.SetString("linedoc_path", 
      // "/mnt/ssd/downloads/enwiki.linedoc_tokenized"); // full article
      // "/mnt/ssd/downloads/enwiki_tookenized_200000.linedoc");
  // config.SetString("loader", "WITH_OFFSETS");
  
  config.SetString("linedoc_path", 
      "/mnt/ssd/downloads/enwiki.linedoc_tokenized.1");
  config.SetString("loader", "WITH_POSITIONS");

  // config.SetString("linedoc_path", 
      // "/mnt/ssd/downloads/enwiki-abstract_tokenized.linedoc");
  // config.SetString("loader", "TOKEN_ONLY");

  config.SetInt("n_repeats", 100);
  config.SetInt("n_passages", 3);
  // config.SetBool("enable_snippets", true);
  config.SetBool("enable_snippets", false);
  
  
  config.SetString("query_source", "hardcoded");
  // config.SetStringVec("terms", std::vector<std::string>{"hello", "world"});
  config.SetStringVec("terms", std::vector<std::string>{"hello"});

  // config.SetString("query_source", "querylog");
  // config.SetString("querylog_path", "/mnt/ssd/downloads/test_querylog");
  return config;
}

GeneralConfig config_by_kan() {
/*
           ____
         / |   |\
  _____/ @ |   | \
 |> . .    |   |   \
  \  .     |||||     \________________________
   |||||||\                                    )
            \                                 |
             \                                |
               \                             /
                |   ____________------\     |
                |  | |                ||    /
                |  | |                ||  |
                |  | |                ||  |
                |  | |                ||  |  Glo Pearl
               (__/_/                ((__/

*/
  GeneralConfig config;
  config.SetInt("n_docs", 10000);
  // config.SetString("linedoc_path", 
      // "/mnt/ssd/downloads/enwiki-abstract_tokenized.linedoc");
  // config.SetString("loader", "with-offsets");

  config.SetString("linedoc_path", 
      "/mnt/ssd/downloads/enwiki-abstract_tokenized.linedoc");
  config.SetString("loader", "naive");

  config.SetInt("n_repeats", 500000);
  config.SetInt("n_passages", 3);
  config.SetBool("enable_snippets", true);
  //config.SetBool("enable_snippets", false);
  
  
  config.SetString("query_source", "hardcoded");
  config.SetStringVec("terms", std::vector<std::string>{"hello"});
  //config.SetStringVec("terms", std::vector<std::string>{"barack", "obama"});
  //config.SetStringVec("terms", std::vector<std::string>{"len", "from", "mai"});
  // config.SetStringVec("terms", std::vector<std::string>{"arsen"});

  // config.SetString("query_source", "querylog");
  // config.SetString("querylog_path", "/mnt/ssd/downloads/test_querylog");
  return config;
}



int main(int argc, char **argv) {
  google::InitGoogleLogging(argv[0]);
  // FLAGS_logtostderr = 1; // print to stderr instead of file
  FLAGS_stderrthreshold = 0; 
  FLAGS_minloglevel = 0; 

  // Separate our configurations to avoid messes
  auto config = config_by_jun();
  // auto config = config_by_kan();
  
  qq_uncompressed_bench_wiki(config);
}


