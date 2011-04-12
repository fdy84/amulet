import zipfile
import sys
import os.path

def zipfile( arg, dirname, names ):
    
    myzip, prefixlen = arg
    
    for n in names :
        
        fn = os.path.join( dirname, n )
        
        if os.path.isfile(fn) :
            
            myzip.write(fn[prefixlen:])
    
    return

def makezipdir( zipname, srcpath ):
    
    srcpath = os.path.join( srcpath, '' )
    
    with ZipFile( zipname, 'w') as myzip:
    
        os.path.walk( srcpath, zipfile, ( myzip, len(srcpath) ) )
    
    return

if __name__ == '__main__' :
    
    makezipdir( sys.argv[1], sys.argv[2] )
    