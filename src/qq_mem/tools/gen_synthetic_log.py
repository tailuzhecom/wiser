import math
import copy
import random
import pickle


"""
exp 0 num of terms: 4996891
exp 1 num of terms: 520675
exp 2 num of terms: 94721
exp 3 num of terms: 22139
exp 4 num of terms: 5717
exp 5 num of terms: 1434
exp 6 num of terms: 38
"""

# DEBUG = True
DEBUG = False

log_to_group = {
        0: "low",
        1: "low",
        2: "low",
        3: "med",
        4: "med",
        5: "high",
        6: "high"}


group_to_log = {}
for log, group in log_to_group.items():
    if group_to_log.has_key(group) is False:
        group_to_log[group] = [log]
    else:
        group_to_log[group].append(log)

print log_to_group
print group_to_log

def rand_item(l):
    total = len(l)
    i = random.randint(0, total - 1)
    return l[i]

def write_to_file(term_set, path):
    with open(path, "w") as f:
        for term in term_set:
            f.write(term + "\n")


class Buckets(object):
    def __init__(self):
        self.buckets = {}
        for i in range(10):
            self.buckets[i] = []
        self.groups = {'low': [], 'med': [], 'high': []}

    def load_term_list(self, path):
        print "loading....."
        f = open(path, "r")

        cnt = 0
        for line in f:
            cnt += 1

            if DEBUG is True:
                if cnt % 10000 != 0:
                    continue

            items = line.split()
            term = items[0]
            doc_freq = int(items[1])
            freq_exp = int(math.log(doc_freq, 10))

            # self.add_to_bucket(freq_exp, term)
            self.add_to_group(freq_exp, term)

        print "Loading finished!"

        f.close()

        self.show_stats()

    def show_stats(self):
        for k, v in self.buckets.items():
            print "exp", k, "num of terms:", len(v)

    def random_pick(self, bucket_index):
        return rand_item(self.buckets[bucket_index])

    def add_to_bucket(self, exp, term):
        self.buckets[exp].append(term)

    def add_to_group(self, exp, term):
        self.groups[log_to_group[exp]].append(term)

    def dump(self, path):
        ret = raw_input("are you sure to overwrite?")
        if ret == "y":
            pickle.dump( self.buckets, open( path, "wb" ) )

    def load(self, path):
        self.buckets = pickle.load( open( path, "rb" ) )

    def dump_bucket(self, i, path):
        random.shuffle(self.buckets[i])
        write_to_file(self.buckets[i], path)

    def create_set(self, bucket_index, n):
        """
        randomly get n terms from buckets[index]
        """
        terms = copy.copy(self.buckets[bucket_index])

        if len(terms) < n:
            raise RuntimeError("not enough terms in this bucket")

        random.shuffle(terms)
        return terms[0:n]

    def create_set_from_group(self, group_name, n):
        """
        randomly get n terms from a group
        """
        terms = copy.copy(self.groups[group_name])

        if n == "all":
            n = len(terms)

        if len(terms) < n:
            raise RuntimeError("not enough terms in this bucket")

        random.shuffle(terms)
        return terms[0:n]


def dump_buckets():
    buckets = Buckets()
    buckets.load_term_list("/mnt/ssd/popular_terms")

    for i in range(0, 7):
        print "dumping", i
        buckets.dump_bucket(i, "unique_terms_1e" + str(i))

def get_queries_from_working_set(buckets, bucket_index, n_uniq_terms, n_queries):
    # pick n_terms (working set), unique ones
    unique_terms = buckets.create_set(bucket_index, n_uniq_terms)

    print "*" * 30
    print "Number of unique terms:", len(unique_terms)
    print "Num of queries to generate:", n_queries
    print "*" * 30

    return produce_queries_from_unique_terms(unique_terms, n_queries)

def produce_queries_from_unique_terms(unique_terms, n_queries):
    n_uniq_terms = len(unique_terms)
    queries = []
    for _ in range(n_queries):
        i = random.randint(0, n_uniq_terms - 1)
        queries.append(unique_terms[i])

    return queries


def produce_non_repeat_queries(buckets):
    pool_sizes = {
                    # 'low':  30,
                    # 'med':  10,
                    # 'high': 20,
                    'low': 300000,
                    'med': "all",
                    'high': 200,
            }

    for group_name in group_to_log.keys():
        print group_name
        queries = buckets.create_set_from_group(group_name, pool_sizes[group_name])
        print "query count:", len(queries)
        write_to_file(queries, "/mnt/ssd/query_workload/single_term/type_single.docfreq_" + group_name)

def main():
    buckets = Buckets()
    buckets.load_term_list("/mnt/ssd/popular_terms")

    produce_non_repeat_queries(buckets)


    # print buckets.groups
    # n_uniq_terms_wanted = [90000, 10000, 1000, 100]
    # for n_wanted in n_uniq_terms_wanted:
        # print "doing", n_wanted
        # queries = get_queries_from_working_set(buckets = buckets,
                                               # bucket_index = 2,
                                               # n_uniq_terms = n_wanted,
                                               # n_queries = 100000)
        # write_to_file(queries, "term_docfreq_1e2_working_set_" + str(n_wanted))





if __name__ == "__main__":
    main()
    # pass


