import os
import sys
import shutil
import os.path
import itertools

PROJECT_DIR = os.path.dirname(os.path.realpath(__file__))

SOURCE_FILES_IDA695 = [
    os.path.join(PROJECT_DIR, 'SimplifyGraph', 'ReleaseIDA695_32', 'SimplifyGraph.plw'),
    os.path.join(PROJECT_DIR, 'SimplifyGraph', 'ReleaseIDA695_64', 'SimplifyGraph.p64'),
]
SOURCE_FILES_IDA70 = [
    os.path.join(PROJECT_DIR, 'SimplifyGraph', 'x64', 'ReleaseIDA70_32', 'SimplifyGraph.dll'),
    os.path.join(PROJECT_DIR, 'SimplifyGraph', 'x64', 'ReleaseIDA70_64', 'SimplifyGraph64.dll'),
]



OUTPATH = os.path.join(PROJECT_DIR, 'builds')

def main():
    if len(sys.argv) != 2:
        print('Usage: python %s <build_version>' % sys.argv[0])
        return
    buildVersion = sys.argv[1]


    dirname1 = os.path.join(OUTPATH, 'SimplifyGraph_' + buildVersion)
    if os.path.exists(dirname1):
        print('Build directory already exists: %s')
        return

    for sfile in itertools.chain(SOURCE_FILES_IDA695, SOURCE_FILES_IDA70):
        if not os.path.exists(sfile):
            print('Source file does not exist: %s' % sfile)
            return
    print('Good to go')
    #dirname2 = os.path.join(dirname1, 'SimplifyGraph')
    dirname3 = os.path.join(dirname1, 'IDA695')
    dirname4 = os.path.join(dirname1, 'IDA70')
    os.mkdir(dirname1)
    #os.mkdir(dirname2)
    os.mkdir(dirname3)
    os.mkdir(dirname4)

    for sfile in SOURCE_FILES_IDA695:
        fname = os.path.basename(sfile)
        dfname = os.path.join(dirname3, fname)
        shutil.copyfile(sfile, dfname)
    
    for sfile in SOURCE_FILES_IDA70:
        fname = os.path.basename(sfile)
        dfname = os.path.join(dirname4, fname)
        shutil.copyfile(sfile, dfname)

    print('Done')
 
if __name__ == '__main__':
    main()
