from pyreuse.fsutils import utils as fsutils
from pyreuse.helpers import *
from bs4 import BeautifulSoup
from lxml import etree




class LineDocPool(object):
    def __init__(self, doc_path):
        self.fd = open(doc_path)

        # Get column names
        # The first line of the doc must be the header
        # Sample header:
        #   FIELDS_HEADER_INDICATOR###      doctitle        docdate body
        header = self.fd.readline()
        self.col_names = header.split("###")[1].split()

    def doc_iterator(self):
        for line in self.fd:
            yield self.line_to_dict(line)

    def line_to_dict(self, line):
        items = line.split("\t")
        return {k:v for k,v in zip(self.col_names, items)}


def setup_dev(devpath, mntpoint):
    fsutils.umount_wait(devpath)

    if "loop" in devpath:
        fsutils.makeLoopDevice(devpath, "/mnt/tmpfs", 1024*32)

    # shcmd("mkfs.ext4 -O ^has_journal -E lazy_itable_init=0,lazy_journal_init=0 {}".format(devpath))
    shcmd("mkfs.ext4 -E lazy_itable_init=0,lazy_journal_init=0 {}".format(devpath))
    prepare_dir(mntpoint)
    # shcmd("mount -o data=writeback {} {}".format(devpath, mntpoint))
    shcmd("mount {} {}".format(devpath, mntpoint))


class QueryPool(object):
    """
    This class is for wiki_QueryLog, which is generated by Kan
    """
    def __init__(self, arg, n):
        if isinstance(arg, str):
            self.fetch_file(arg, n)
        elif isinstance(arg, list):
            self.queries = arg
            self.n = len(arg)

        self.i = 0

    def fetch_file(self, query_path, n):
        self.fd = open(query_path, 'r')
        self.n = n
        # self.queries = self.fd.readlines(n)
        self.queries = []

        while True:
            line = self.fd.readline()
            if line == "":
                break

            self.queries.append(line)
            n -= 1
            if n == 0:
                break

    def next_query(self):
        query = self.queries[self.i]
        self.i = (self.i + 1) % self.n

        query = query.replace("&language=en", " ")
        query = re.sub("[^\w]", " ",  query)
        query = query.replace("AND", " ")
        # format: "hello world"
        return query


class WikiAbstract(object):
    """
    This class allows you to iterate entries in wikipedia abstract
    """
    def __init__(self, path):
        with open(path) as f:
            self.soup = BeautifulSoup(f, "lxml")

    def entries(self):
        docs = self.soup.feed.find_all("doc")
        for doc in docs:
            yield {"title": doc.title.string, "abstract": doc.abstract.string}


class WikiAbstract2(object):
    """
    This class allows you to iterate entries in wikipedia abstract
    """
    def __init__(self, path):
        self.path = path

    def entries(self):
        for event, element in etree.iterparse(self.path, tag="doc"):
            yield {'title': element.findtext('title'),
                   'abstract': element.findtext('abstract')}
            element.clear()


