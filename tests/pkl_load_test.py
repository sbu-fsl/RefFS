#!/usr/bin/env python3
import glob
import subprocess
import os
import sys
import time
import errno

# config
fs_name = 'fuse-cpp-ramfs'
path_to_fs = 'mnt/fuse-cpp-ramfs'
absfs_path = '../../fs-state/absfs'
pkl_path = '../build/pkl'
load_path = '../build/load'
#

print(sys.platform)

config_list = []
size = 0


def make_sure_path_exists(path):
    try:
        os.makedirs(path)
    except OSError as exception:
        if exception.errno != errno.EEXIST:
            raise


def get_fs_signature():
    p1 = subprocess.Popen([absfs_path, path_to_fs], stdout=subprocess.PIPE)
    p2 = subprocess.Popen(['awk', 'END{print $NF}'], stdin=p1.stdout, stdout=subprocess.PIPE)
    return p2.communicate()[0]


def pickle_save_signature():
    global size
    config_list.append(get_fs_signature())
    subprocess.run([pkl_path, path_to_fs, 'pickle_tmp{}'.format(size)])
    size += 1


def load_verify_signature(i):
    global size
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


# ===================================================


def perform_test():
    for i in range(100):
        run(['mkdir', 'pickle_dir_1_{}'.format(i)])
        run(['touch', 'pickle_file_1_{}'.format(i)])
    pickle_save_signature()

    time.sleep(1)

    for i in range(100, 200):
        run(['mkdir', 'pickle_dir_1_{}'.format(i)])
        run(['touch', 'pickle_file_1_{}'.format(i)])
    load_verify_signature(0)

    for i in range(100):
        run(['mkdir', 'pickle_dir_2_{}'.format(i)], '{}/pickle_dir_1_{}'.format(path_to_fs, i))
        for j in range(5):
            run(['touch', 'pickle_file_2_{}'.format(j)],
                '{}/pickle_dir_1_{}/pickle_dir_2_{}'.format(path_to_fs, i, i))
            run(['chmod','a+x', 'pickle_file_2_{}'.format(j)],
                '{}/pickle_dir_1_{}/pickle_dir_2_{}'.format(path_to_fs, i, i))

    pickle_save_signature()

    time.sleep(1)

    for i in range(100):
        run(['rm', '-rf', 'pickle_dir_2_{}'.format(i)], '{}/pickle_dir_1_{}'.format(path_to_fs, i))

    load_verify_signature(1)

    for i in range(100):
        run(['mv', 'pickle_dir_1_{}'.format(i),'pickle_new_dir_1_{}'.format(i)])
    run(['mv', 'pickle_new_dir_1_0','pickle_new_dir_1_2/pickle_new_dir_1_0'])

    pickle_save_signature()

    time.sleep(1)

    run(['rm', '-rf', '*'])

    load_verify_signature(2)


# ===================================================

make_sure_path_exists(path_to_fs)

clean_files('', path_to_fs)

child = subprocess.Popen([fs_name,
                          path_to_fs])

time.sleep(1)

perform_test()

# If you unmount too soon, the mountpoint won't be available.

if sys.platform == 'darwin' or sys.platform == 'linux':
    subprocess.run(['umount',
                    path_to_fs])
else:
    subprocess.run(['fusermount', '-u',
                    path_to_fs])

print('perform cleanup......')

child.wait()

for fl in glob.glob('pickle_tmp*'):
    os.remove(fl)

sys.exit(child.returncode)
