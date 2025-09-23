# builtin
import logging
import os
import os.path
import shutil
import subprocess
import uuid
# h3gui
import nd_consts


def launch_chrome():
    # first clear the user-data-dir dirs before creating a new one...
    data_dir_root = os.path.join(nd_consts.ND_ROOT_DIR, 'dat', 'user_data_dir')
    if os.path.isdir(data_dir_root):
        logging.info(f'Clearing {data_dir_root}')
        shutil.rmtree(data_dir_root)
    # unique dir name
    new_data_dir = os.path.join(data_dir_root, uuid.uuid4().hex)
    logging.info(f'Creating {new_data_dir}')
    os.makedirs(new_data_dir)
    chrome_launch_dict = nd_consts.CHROME_LAUNCH_DICT.copy()
    chrome_launch_dict['user_data_dir'] = new_data_dir
    launch_cmd = nd_consts.CHROME_LAUNCH_FMT % chrome_launch_dict
    logging.info(f'Chrome launch: {launch_cmd}')
    chrome_proc = subprocess.Popen(launch_cmd)


if __name__ == '__main__':
    logging.getLogger().addHandler(logging.StreamHandler())
    launch_chrome()

