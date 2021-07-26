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
fs_name = 'fuse-cpp-ramfs'
path_to_fs = '{}/mnt/fuse-cpp-ramfs'.format(os.getcwd())  # absolute path only
absfs_path = '../../fs-state/absfs'
pkl_path = '../build/pkl'
load_path = '../build/load'
'''*You need to comment out (rm -rf "$DIR") in racer script for this to work correctly'''
racer_script_path = 'racer/racer.sh'
'''range format: [a,b)'''
racer_duration = (3, 6)
racer_threads = (1, 2)
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

        pickle_save_signature()

        time.sleep(3)

        p = subprocess.Popen([racer_script_path,
                              path_to_fs,
                              str(get_random_in_range(racer_duration)),
                              str(get_random_in_range(racer_threads))])

        p.wait()

        load_verify_signature(size - 1)


# ===================================================

make_sure_path_exists(path_to_fs)

clean_files('', path_to_fs)

child = subprocess.Popen([fs_name,
                          path_to_fs])

time.sleep(1)

try:
    perform_test()
except Exception as err:
    print(err)
    if p is not None:
        pid = p.pid
        with suppress(Exception):
            os.kill(pid, signal.SIGINT)  # or SIGINT to CTRL_C_EVENT for Windows
    pass

clean_exit()

child.wait()
sys.exit(child.returncode)
