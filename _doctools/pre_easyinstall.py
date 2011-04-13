
import urllib
import re
import os.path
import tempfile
import tarfile

downurl = 'http://peak.telecommunity.com/snapshots/'

def getwebpage( url ):

    f = urllib.urlopen( url )
    
    try :
        return f.read()
    finally :
        f.close()
        


def findpkg( url ):
    
    pkglist = getwebpage( url )
    
    pkglist = re.findall(r"setuptools\-.*?\.tar\.gz", pkglist )
    
    pkglist.sort()
    
    return pkglist[-1]
    
def downloadpkg( url, path, fn ):
    
    absfn = os.path.join(path,fn)
    
    urllib.urlretrieve( url, absfn )
    
    fp = tarfile.open( absfn, mode="r:gz" )
    fp.extractall( path )
    
    return
    
def installpkg( path ):
    
    import sys
    import os
    import shutil
    
    os.chdir( path ) 
    
    sys.path = [ path ] + sys.path
    sys.argv = ['setup.py','install']
    
    import setup
    
    shutil.rmtree( path )
    
    return
    
if __name__ == '__main__':
    
    tempdir = tempfile.mkdtemp()
    
    fn = findpkg( downurl )
    
    downloadpkg( downurl + fn , tempdir, fn )
    
    installpkg( os.path.join(tempdir,fn.rsplit('.',2)[0]) )
    
