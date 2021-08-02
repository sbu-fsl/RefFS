#!/usr/bin/env python3
import glob
import subprocess
import os
import sys
import time
import errno
import random
import signal
from contextlib import suppress

# config
fs_exec = 'fuse-cpp-ramfs'  # bin program or path, ex: ../../verifs1/crmfs
path_to_fs = '{}/mnt/fuse-cpp-ramfs'.format(os.getcwd())  # absolute path only
absfs_path = '../../fs-state/absfs'
pkl_path = '../build/pkl'
load_path = '../build/load'
'''*You need to comment out (rm -rf "$DIR") in racer script for this to work correctly'''
racer_script_path = 'racer/racer.sh'
'''range format: [a,b)'''
racer_duration = (3, 7)
racer_threads = (3, 5)
num_test_attempts = (1, 2)
#

print(sys.platform)

config_list = []
size = 0
p = None


def get_random_in_range(range_tpl):
    return random.randrange(range_tpl[0], range_tpl[1])


def make_sure_path_exists(path):
    try:
        os.makedirs(path)
    except OSError as exception:
        if exception.errno != errno.EEXIST:
            raise


def get_fs_signature():
    p1 = subprocess.Popen([absfs_path, path_to_fs], stdout=subprocess.PIPE)
    p2 = subprocess.Popen(['awk', 'END{print $NF}'], stdin=p1.stdout, stdout=subprocess.PIPE)
    signature = p2.communicate()[0]
    if signature == b'iterating...\n':
        p1 = subprocess.Popen([absfs_path, path_to_fs], stdout=subprocess.PIPE)
        raise Exception(p1.communicate()[0])
    return signature


def pickle_save_signature():
    global size
    signature = get_fs_signature()
    config_list.append(signature)
    subprocess.run([pkl_path, path_to_fs, 'pickle_tmp{}'.format(size)])
    print('saved pickle file at index {}, signature: {}'.format(size, signature))
    size += 1


def load_verify_signature(i):
    subprocess.run([load_path, path_to_fs, 'pickle_tmp{}'.format(i)])
    signature = config_list[i]
    time.sleep(1)
    new_signature = get_fs_signature()
    if new_signature == signature:
        print('loaded pickle file at index {} passed verification, signature: {}'.format(i, signature))
    else:
        raise Exception('signature at index {} has a discrepancy: {} -> {}'.format(i, signature, new_signature))


def clean_files(prefix, cwd=None):
    if prefix == '/' or (prefix == '' and cwd is None):
        raise Exception('dangerous action')
    subprocess.run(['rm', '-rf', '{}*'.format(prefix)], cwd=cwd)


def run(cmd, cwd=path_to_fs):
    subprocess.run(cmd, cwd=cwd)


def clean_exit():
    time.sleep(1)
    # If you unmount too soon, the mountpoint won't be available.
    if sys.platform == 'darwin' or sys.platform == 'linux':
        subprocess.run(['umount',
                        path_to_fs])
    else:
        subprocess.run(['fusermount', '-u',
                        path_to_fs])

    print('perform cleanup......')

    for fl in glob.glob('pickle_tmp*'):
        os.remove(fl)


# ===================================================


def perform_test():
    global p
    for i in range(get_random_in_range(num_test_attempts)):
        time.sleep(3)
        p = subprocess.Popen([racer_script_path,
                              path_to_fs,
                              str(get_random_in_range(racer_duration)),
                              str(get_random_in_range(racer_threads))])

        p.wait()

        input('make sure racer threads are terminated and press any button to continue:')

        pickle_save_signature()

        input('press any button to load the pickled file:')

        load_verify_signature(size - 1)


# ===================================================

make_sure_path_exists(path_to_fs)

clean_files('', path_to_fs)

child = subprocess.Popen([fs_exec,
                          path_to_fs])

time.sleep(1)

try:
    perform_test()
    input()
except KeyboardInterrupt:
    pass
except Exception as err:
    print(err)
    if p is not None:
        pid = p.pid
        with suppress(Exception):
            os.kill(pid, signal.SIGINT)  # or SIGINT to CTRL_C_EVENT for Windows
    input()
    pass

clean_exit()

child.wait()
sys.exit(child.returncode)
