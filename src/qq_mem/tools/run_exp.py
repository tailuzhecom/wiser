import sys
import subprocess, os
import time
import shlex
import signal
from pyreuse.helpers import *
from pyreuse.macros import *
from pyreuse.sysutils.cgroup import *
from pyreuse.sysutils.iostat_parser import parse_iostat
from pyreuse.general.expbase import *
from pyreuse.sysutils.blocktrace import BlockTraceManager
import glob
import pprint

ELASTIC = "elastic"
ELASTIC_PY = "elasticpy"
VACUUM = "vacuum"

######################
# System wide
######################
server_addr = "node1"
remote_addr = "node2"
os_swap = True
device_name = "nvme0n1"
partition_name = "nvme0n1p4"
do_drop_cache = True
do_block_tracing = False
qq_mem_folder = "/users/jhe/flashsearch/src/qq_mem"
user_name = "jhe"
read_ahead_kb_list = [32]


######################
# BOTH Elastic and Vacuum
######################
# engines = [ELASTIC] # ELASTIC or VACUUM
engines = [ELASTIC_PY] # ELASTIC or VACUUM
# engines = [VACUUM] # ELASTIC or VACUUM
n_server_threads = [25]
n_client_threads = [128] # client
# mem_size_list = [8*GB, 4*GB, 2*GB, 1*GB, 512*MB, 256*MB, 128*MB] # good one
# mem_size_list = [8*GB, 4*GB]
# mem_size_list = [8*GB, 4*GB, 2*GB, 1*GB, 512*MB, 256*MB, 128*MB] # good one
# mem_size_list = [512*MB, 256*MB, 128*MB] # good one
# mem_size_list = [256*MB]
mem_size_list = [8*GB]

# Make the a pair?
enable_prefetch_list = [True] # whether to eanble Vaccum to do prefetch
prefetch_thresholds_kb = [128] # unit is KB


mem_swappiness = 60
# query_paths = ["/mnt/ssd/querylog_no_repeated"]
# query_paths = ["/mnt/ssd/realistic_querylog"]
# query_paths = ["/mnt/ssd/by-doc-freq/unique_terms_1e2"]
# query_paths = ["/mnt/ssd/realistic_querylog"]
# query_paths = ["/mnt/ssd/query_workload/from_log"]
# query_paths = ["/mnt/ssd/query_workload/type_realistic"]
query_paths = ["/mnt/ssd/short_log"]
# query_paths = ["/mnt/ssd/query_workload/by-doc-freq/type_fiveplus"]
# query_paths = ["/mnt/ssd/query_workload/single_term/type_single.docfreq_high"]
# query_paths = glob.glob("/mnt/ssd/query_workload/single_term/type_single.docfreq_high")
# query_paths = glob.glob("/mnt/ssd/query_workload/single_term/*") +\
# query_paths = glob.glob("/mnt/ssd/query_workload/two_term/type_twoterm") +\
 # glob.glob("/mnt/ssd/query_workload/two_term_phrases/type_phrase") +\
 # ["/mnt/ssd/query_workload/type_realistic"]
# query_paths = glob.glob("/mnt/ssd/query_workload/two_term/type_twoterm")
# query_paths = glob.glob("/mnt/ssd/query_workload/two_term_phrases/type_phrase")
lock_memory = ["false"] # must be string


######################
# Vacuum only
######################
search_engine = "vacuum:vacuum_dump:/mnt/ssd/vacuum_0501_prefetch_zone"
# search_engine = "vacuum:vacuum_dump:/mnt/ssd/vacuum_delta_skiplist-04-30"
# search_engine = "vacuum:vacuum_dump:/mnt/ssd/vacuum-files-aligned-fdt"
# search_engine = "vacuum:vacuum_dump:/mnt/ssd/vacuum-files-misaligned-fdt"
profile_qq_server = "false"
engine_path = "/users/jhe/flashsearch/src/qq_mem/build/engine_bench"

######################
# Elastic only
######################
ELASTIC_DIR = "/users/jhe/elasticsearch-5.6.3"
init_heap_size = [300*MB]
max_heap_size = [300*MB]
rs_bench_go_path = "/users/jhe/flashsearch/src/pysrc"











if device_name == "sdc":
    partition_padding_bytes = 0
elif device_name == "nvme0n1":
    partition_padding_bytes = 46139392 * 512

gprof_env = os.environ.copy()
gprof_env["CPUPROFILE_FREQUENCY"] = '1000'

class VacuumClientOutput:
    def _parse_perf(self, header, data_line):
        header = header.split()
        items = data_line.split()

        print header
        print items

        return dict(zip(header, items))

    def parse_client_out(self, path):
        with open(path) as f:
            lines = f.readlines()

        for i, line in enumerate(lines):
            if "latency_95th" in line:
                return self._parse_perf(lines[i], lines[i+1])


class ElasticClientOutput:
    def _parse_throughput(self, line):
        qps = line.split()[1]
        line = line.replace(",", " ")
        return {"QPS": qps.split(".")[0]}

    def _parse_latencies(self, line):
        line = line.replace(",", " ")
        items = line.split()
        return {
                "latency_50th": items[1],
                "latency_95th": items[2],
                "latency_99th": items[3]}

    def parse_client_out(self, path):
        with open(path) as f:
            lines = f.readlines()

        d = {}
        for i, line in enumerate(lines):
            print line
            if line.startswith("Throughput:"):
                d.update(self._parse_throughput(lines[i]))
            elif line.startswith("Latencies:"):
                d.update(self._parse_latencies(lines[i]))

        return d

def parse_client_output(conf):
    if conf['engine'] == ELASTIC:
        return ElasticClientOutput().parse_client_out("/tmp/client.out")
    elif conf['engine'] == VACUUM:
        return VacuumClientOutput().parse_client_out("/tmp/client.out")
    elif conf['engine'] == ELASTIC_PY:
        return {}
    else:
        raise RuntimeError

def warmup_elastic():
    # p = start_engine_client("elastic", 1, "/mnt/ssd/query_workload/debug/warmup_log")
    # raw_input("waiting for client to warmup server")
    # p.wait()
    p = start_elastic_pyclient("/mnt/ssd/query_workload/warmup_log")
    p.wait()
    shcmd("rm -f /tmp/client.out")
    time.sleep(1)


class Cgroup(object):
    def __init__(self, name, subs):
        self.name = name
        self.subs = subs

        shcmd('sudo cgcreate -g {subs}:{name}'.format(subs=subs, name=name))

    def set_item(self, sub, item, value):
        """
        Example: sub='memory', item='memory.limit_in_bytes'
        echo 3221225472 > memory/confine/memory.limit_in_bytes
        """
        path = self._path(sub, item)
        shcmd("echo {value} |sudo tee {path}".format(value = value, path = path))

        # self._write(sub, item, value)

        ret_value = self._read(sub, item)

        if ret_value != str(value):
            print 'Warning:', ret_value, '!=', value

    def get_item(self, sub, item):
        return self._read(sub, item)

    def execute(self, cmd):
        cg_cmd = ['sudo', 'cgexec',
                '-g', '{subs}:{name}'.format(subs=self.subs, name=self.name),
                '--sticky']
        cg_cmd += cmd
        print cg_cmd
        p = subprocess.Popen(cg_cmd, preexec_fn=os.setsid)

        return p

    def _write(self, sub, item, value):
        path = self._path(sub, item)
        with open(path, 'w') as f:
            f.write(str(value))

    def _read(self, sub, item):
        path = self._path(sub, item)
        with open(path, 'r') as f:
            value = f.read()

        return value.strip()

    def _path(self, sub, item):
        path = os.path.join('/sys/fs/cgroup', sub, self.name, item)
        return path


def get_cgroup_page_cache_size():
    def parse_page_cache(line):
        cache_size = line.split()[2]
        d = {"page_cache_size": cache_size}
        return d

    lines = subprocess.check_output(
            "cgget -g memory:/charlie", shell=True).split('\n')

    d = {}
    for i, line in enumerate(lines):
        if line.startswith("memory.stat: cache"):
            d.update(parse_page_cache(lines[i]))

    return int(d['page_cache_size'])


def clean_dmesg():
    shcmd("sudo dmesg -C")

def is_port_open(proc_name):
    cmd = "sudo netstat -ap |grep {} | wc -l".format(proc_name)
    ret = subprocess.check_output(cmd, shell=True).strip()
    n = int(ret)
    return n > 0

def wait_for_open_port(proc_name):
    print "Waiting open port of", proc_name, "......"
    while is_port_open(proc_name) is False:
        time.sleep(2)
    time.sleep(2) # always wait for a while

def wait_engine_port(conf):
    if conf['engine'] == ELASTIC:
        wait_for_open_port("java")
    elif conf['engine'] == ELASTIC_PY:
        wait_for_open_port("java")
    elif conf['engine'] == VACUUM:
        wait_for_open_port("qq_server")
    else:
        raise RuntimeError

def set_es_yml(conf):
    with open(os.path.join(
        ELASTIC_DIR, "config/elasticsearch.yml.template"), "r") as f:
        lines = f.readlines()

    new_lines = []
    for line in lines:
        if "thread_pool.search.size" in line:
            new_lines.append(
                "thread_pool.search.size: {}\n".format(conf['n_server_threads']))
        elif "bootstrap.memory_lock" in line:
            new_lines.append(
                "bootstrap.memory_lock: {}\n".format(conf['lock_memory']))
        else:
            new_lines.append(line)

    with open(os.path.join(ELASTIC_DIR, "config/elasticsearch.yml"), "w") as f:
        f.write("".join(new_lines))

def set_jvm_options(conf):
    with open(os.path.join(ELASTIC_DIR, "config/jvm.options.template"), "r") as f:
        lines = f.readlines()

    new_lines = []
    for line in lines:
        if "init_heap_size" in line:
            new_lines.append(
                "-Xms{}\n".format(conf['init_heap_size']))
        elif "max_heap_size" in line:
            new_lines.append(
                "-Xmx{}\n".format(conf['max_heap_size']))
        else:
            new_lines.append(line)

    with open(os.path.join(ELASTIC_DIR, "config/jvm.options"), "w") as f:
        f.write("".join(new_lines))


def log_crashed_server(conf):
    with open("server.log", "a") as f:
        f.write("server crashed at " + now() + " ")
        f.write(repr(conf) + "\n")


def log_crashed_client(conf):
    with open("server.log", "a") as f:
        f.write("CLIENT crashed at " + now() + " ")
        f.write(repr(conf) + "\n")


def send_sigint(pid):
    shcmd("bash -c 'sudo kill -SIGINT {}'".format(os.getpgid(pid)))
    # os.killpg(os.getpgid(pid), signal.SIGINT)

def get_iostat_kb_read():
    out = subprocess.check_output("iostat -k {}".format(partition_name), shell=True)
    print out
    return parse_iostat(out)['io']['kB_read']

def create_blktrace_manager(subexpdir):
    return BlockTraceManager(
            dev = os.path.join("/dev", partition_name),
            event_file_column_names = ['pid', 'operation', 'offset', 'size',
                'timestamp', 'pre_wait_time'],
            resultpath = os.path.join(subexpdir, "raw-trace.txt"),
            to_ftlsim_path = os.path.join(subexpdir, "trace-table.txt"),
            sector_size = 512,
            padding_bytes = partition_padding_bytes,
            do_sort = False)

def kill_blktrace():
    shcmd("sudo pkill blktrace", ignore_error = True)

def drop_cache():
    shcmd("sudo dropcache")

def set_swap(val):
    if val == True:
        print "-" * 20
        print "Enabling swap..."
        print "-" * 20
        shcmd("sudo swapon -a")
    else:
        print "-" * 20
        print "Disabling swap..."
        print "-" * 20
        shcmd("sudo swapoff -a")

def set_read_ahead_kb(val):
    path = "/sys/block/{}/queue/read_ahead_kb".format(device_name)
    shcmd("echo {} |sudo tee {}".format(val, path))

def check_vacuum_port():
    print "-" * 20
    print "Port of qq server"
    shcmd("sudo netstat -ap | grep qq_server")
    print "-" * 20

def check_es_port():
    print "-" * 20
    print "Port of elasticsearch "
    shcmd("sudo netstat -ap | grep java")
    print "-" * 20

def check_server_port(engine):
    if engine == ELASTIC:
        check_es_port()
    elif engine == VACUUM:
        check_vacuum_port()
    else:
        raise RuntimeError

def remote_cmd_chwd(dir_path, cmd, fd = None):
    return remote_cmd("cd {}; {}".format(dir_path, cmd), fd)

def remote_cmd(cmd, fd = None):
    print "Starting command on remote node.... ", cmd
    if fd is None:
        p = subprocess.Popen(["ssh", remote_addr, cmd])
    else:
        p = subprocess.Popen(["ssh", remote_addr, cmd], stdout = fd, stderr = fd)

    return p

def sync_build_dir():
    if server_addr == remote_addr:
        return

    shcmd("scp {engine_path} {addr}:/tmp/engine_bench".format(
        addr=remote_addr, engine_path=engine_path))

def sync_query_log(path):
    if server_addr == remote_addr:
        return

    dirpath = os.path.dirname(path)
    p = remote_cmd("mkdir -p {}".format(dirpath))
    p.wait()

    shcmd("rsync -avzh --progress {path} {addr}:{path}".format(
        addr=remote_addr, path=path))

def compile_engine_bench():
    shcmd("make engine_bench_build")


def start_engine_client(engine, n_threads, query_path):
    if engine == ELASTIC:
        return start_elastic_client(n_threads, query_path)
    elif engine == ELASTIC_PY:
        return start_elastic_pyclient(query_path)
    elif engine == VACUUM:
        return start_vacuum_client(n_threads, query_path)

def start_elastic_pyclient(query_path):
    print "-" * 20
    print "starting client ..."
    print "-" * 20
    cmd = "sudo python benchmarks/es_client.py {path}"\
            .format(path=query_path)

    f = open("/tmp/client.out", "w")
    with cd(rs_bench_go_path):
        return subprocess.Popen(cmd, shell=True, stdout = f, stderr = f)


def start_elastic_client(n_threads, query_path):
    print "-" * 20
    print "starting client ..."
    print "-" * 20
    cmd = "sudo python -m benchmarks.rs_bench_go {path} {n} {server}"\
            .format(path=query_path, n=n_threads, server=server_addr)

    return remote_cmd_chwd(
        rs_bench_go_path,
        cmd,
        open("/tmp/client.out", "w")
        )

def start_vacuum_client(n_threads, query_path):
    print "-" * 20
    print "starting client ..."
    print "-" * 20
    return remote_cmd_chwd(
        "/tmp/",
        "./engine_bench -exp_mode=grpclog -n_threads={n_threads} "
        "-grpc_server={server} -query_path={query_path}"
        .format(server = server_addr, n_threads = n_threads,
           query_path = query_path),
        open("/tmp/client.out", "w")
        )

def print_client_output_tail():
    out = subprocess.check_output("tail -n 2 /tmp/client.out", shell=True)
    print out

def is_client_running(conf):
    if conf['engine'] == ELASTIC:
        return is_elastic_client_running()
    elif conf['engine'] == ELASTIC_PY:
        return is_elastic_pyclient_running()
    elif conf['engine'] == VACUUM:
        return is_vacuum_client_running()
    else:
        raise RuntimeError

def is_vacuum_client_running():
    p = remote_cmd("ps -ef|grep engine_bench|wc -l", subprocess.PIPE)
    out = p.communicate()[0]
    print out
    return int(out) > 2 # one of them is the grep, one is the ssh..

def is_elastic_client_running():
    p = remote_cmd("ps -ef|grep RediSearchBench|wc -l", subprocess.PIPE)
    out = p.communicate()[0]
    print out
    return int(out) > 2 # one of them is the grep


def is_elastic_pyclient_running():
    p = remote_cmd("ps -ef|grep es_client|wc -l", subprocess.PIPE)
    out = p.communicate()[0]
    print out
    return int(out) > 2 # one of them is the grep


def is_client_finished():
    out = subprocess.check_output("tail -n 1 /tmp/client.out", shell=True)
    print "------> Contents of client.out: ", out
    if "ExperimentFinished!!!" in out:
        return True
    else:
        return False

def median(lst):
    sortedLst = sorted(lst)
    lstLen = len(lst)

    if lstLen == 0:
        return 0

    index = (lstLen - 1) // 2

    if (lstLen % 2):
        return sortedLst[index]
    else:
        return (sortedLst[index] + sortedLst[index + 1])/2.0


def kill_client():
    p = remote_cmd("sudo pkill RediSearchBench")
    p.wait()
    p = remote_cmd("sudo pkill engine_bench")
    p.wait()
    shcmd("sudo pkill es_client.py", ignore_error = True)

def kill_server():
    shcmd("sudo pkill qq_server", ignore_error=True)
    shcmd("sudo pkill java", ignore_error=True)

def copy_client_out():
    prepare_dir("/tmp/results")
    shcmd("cp /tmp/client.out /tmp/results/{}".format(now()))

def start_engine_server(conf):
    if conf['engine'] == ELASTIC:
        return start_elastic_server(conf)
    elif conf['engine'] == ELASTIC_PY:
        return start_elastic_server(conf)
    elif conf['engine'] == VACUUM:
        return start_vacuum_server(conf)
    else:
        raise RuntimeError

def start_vacuum_server(conf):
    print "-" * 20
    print "starting server ..."
    print "server mem size:", conf['server_mem_size']
    print "-" * 20

    cg = Cgroup(name='charlie', subs='memory')
    cg.set_item('memory', 'memory.limit_in_bytes', conf['server_mem_size'])
    cg.set_item('memory', 'memory.swappiness', mem_swappiness)

    with cd(qq_mem_folder):
        cmd = "./build/qq_server "\
              "-sync_type=ASYNC -n_threads={n_threads} "\
              "-addr={server} -port=50051 -engine={engine} -profile_vacuum={profile} "\
              "-enable_prefetch={enable_prefetch} -prefetch_threshold={threshold} "\
              "-lock_memory={lock_mem}"\
              .format(server = server_addr,
                    n_threads = conf['n_server_threads'],
                    engine = search_engine,
                    enable_prefetch = conf['enable_prefetch'],
                    threshold = conf['prefetch_threshold_kb'] / 4,
                    profile = profile_qq_server,
                    lock_mem = conf['lock_memory'])
        print "-" * 20
        print "server cmd:", cmd
        print "-" * 20
        # p = subprocess.Popen(shlex.split(cmd), env = gprof_env)
        p = cg.execute(shlex.split(cmd))

        time.sleep(2)

        return p

def start_elastic_server(conf):
    print "-" * 20
    print "starting server ..."
    print "server mem size:", conf['server_mem_size']
    print "-" * 20

    cg = Cgroup(name='charlie', subs='memory')
    cg.set_item('memory', 'memory.limit_in_bytes', conf['server_mem_size'])
    cg.set_item('memory', 'memory.swappiness', mem_swappiness)

    p = cg.execute([
        'sudo', '-u', user_name, '-H', 'sh', '-c',
        os.path.join(ELASTIC_DIR, "bin/elasticsearch")])
    time.sleep(1)
    return p

def is_engine_server_running(conf):
    if conf['engine'] in (ELASTIC, ELASTIC_PY):
        return is_command_running("/usr/lib/jvm/java-8-oracle/bin/java")
    elif conf['engine'] == VACUUM:
        return is_command_running("./build/qq_server")
    else:
        raise RuntimeError

def parse_filename(path):
    """
    example
    path = "/mnt/ssd/query_workload/single_term/type_single.docfreq_low"
    {'type': 'single', 'docfreq': 'low'}
    """
    filename = os.path.basename(path)
    items = [ y for x in filename.split(".") for y in x.split("_") ]
    keys = items[0::2]
    values = items[1::2]

    d = dict(zip(keys, values))
    return d

class Exp(Experiment):
    def __init__(self):
        self._exp_name = "exp-" + now()

        self.confs = parameter_combinations({
                "server_mem_size": mem_size_list,
                "n_server_threads": n_server_threads,
                "n_client_threads": n_client_threads,
                "query_path": query_paths,
                "engine": engines,
                "init_heap_size": init_heap_size,
                "max_heap_size": max_heap_size,
                "lock_memory": lock_memory,
                "read_ahead_kb": read_ahead_kb_list,
                "prefetch_threshold_kb": prefetch_thresholds_kb,
                "enable_prefetch": enable_prefetch_list
                })
        self._n_treatments = len(self.confs)
        pprint.pprint(self.confs)
        print "Number of treatments: ", self._n_treatments
        raw_input("This is your config. Enter to continue")

        self.result = []

    def conf(self, i):
        return self.confs[i]

    def before(self):
        clean_dmesg()

        engines = [conf['engine'] for conf in self.confs]
        if 'vacuum' in engines:
            compile_engine_bench()

        kill_client()
        sync_build_dir()

    def beforeEach(self, conf):
        kill_server()
        kill_blktrace()

        kill_client()
        time.sleep(1)
        shcmd("rm -f /tmp/client.out")

        if conf['engine'] == ELASTIC:
            set_es_yml(conf)
            set_jvm_options(conf)
        elif conf['engine'] == VACUUM:
            pass
        elif conf['engine'] == ELASTIC_PY:
            pass
        else:
            raise RuntimeError("Wrong engine")

        set_swap(os_swap)
        set_read_ahead_kb(conf['read_ahead_kb'])
        if do_drop_cache == True:
            drop_cache()

        sync_query_log(conf['query_path'])

    def treatment(self, conf):
        print conf
        server_p = start_engine_server(conf)

        print "Wating for some time util the server starts...."
        print "Wait for server to load index, ..."
        time.sleep(15)

        wait_engine_port(conf)
        kb_read_a = get_iostat_kb_read()

        # Start block tracing after the server is fully loaded
        if do_block_tracing is True:
            self.blocktracer = create_blktrace_manager(self._subexpdir)
            self.blocktracer.start_tracing_and_collecting(['issue'])
        print "Wait for client to fully start (1 sec)..."
        time.sleep(1)

        client_p = start_engine_client(
                conf['engine'], conf['n_client_threads'], conf['query_path'])

        if conf['engine'] == ELASTIC:
            print "Waiting for client to start...."
            time.sleep(5)

        seconds = 0
	cache_size_log = []
        while True:
            print_client_output_tail()
            print conf

            is_server_running = is_engine_server_running(conf)
            if is_server_running is False:
                # raise RuntimeError("Server just crashed!!!")
                print "!" * 40
                print "Server crashed. See server.log and dmesg for details"
                print "!" * 40
                log_crashed_server(conf)
                return

            is_running = is_client_running(conf)
            if is_running is False:
                sys.stdout.flush()
                kill_client() # hoping the client output will be flushed
                print "Wait for the client to flush stdout........"
                time.sleep(30)

            finished = is_client_finished()
            print "is_client_finished()", finished
            if finished:
                kill_client()
                if conf['engine'] == 'vacuum':
                    send_sigint(server_p.pid)
                print "Waiting for server to exit gracefuly"
                time.sleep(8)
                copy_client_out()
                break

            # client is not finished, and not running
            if is_running is False:
                print "!" * 40
                print "Client is not running!"
                print "!" * 40
                log_crashed_client(conf)
                return

            cache_size = get_cgroup_page_cache_size()/MB
            print "*" * 20
            print "page cache size (MB): ", cache_size
            print "*" * 20
            cache_size_log.append(cache_size)

            time.sleep(1)
            seconds += 1
            print ">>>>> It has been", seconds, "seconds <<<<<<"
            print "cache_size_log:", cache_size_log

        kb_read_b = get_iostat_kb_read()

        kb_read = int(kb_read_b) - int(kb_read_a)
        print '-' * 30
        print "KB read: ", kb_read
        print '-' * 30

        d = parse_client_output(conf)
        if len(cache_size_log) == 0:
            d["cache_mb_obs_median"] = "NA"
        else:
            d["cache_mb_obs_median"] = median(cache_size_log)
        d["cache_mb_max"] = max(cache_size_log + [0])
        d['KB_read'] = kb_read

        filename_dict = parse_filename(conf['query_path'])

        d.update(filename_dict)
        d.update(conf)

        self.result.append(d)

    def afterEach(self, conf):
        path = os.path.join(self._resultdir, "result.txt")
        table_to_file(fill_na(self.result), path, width = 20)
        shcmd("cat " + path)

        if do_block_tracing is True:
            shcmd("sync")
            self.blocktracer.stop_tracing_and_collecting()
            time.sleep(2)
            self.blocktracer.create_event_file_from_blkparse()
        dump_json(conf, os.path.join(self._subexpdir, "config.json"))

if __name__ == "__main__":
    exp = Exp()
    exp.main()


