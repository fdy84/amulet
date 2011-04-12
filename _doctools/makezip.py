import zipfile
import sys
import os.path

def ziponefile( arg, dirname, names ):
    
    myzip, prefixlen = arg
    
    for n in names :
        
        fn = os.path.join( dirname, n )
        
        if os.path.isfile(fn) :
            
            myzip.write(fn[prefixlen:])
    
    return

def makezipdir( zipname, srcpath ):
    
    srcpath = os.path.join( srcpath, '' )
    
    with zipfile.ZipFile( zipname, 'w') as myzip:
    
        os.path.walk( srcpath, ziponefile, ( myzip, len(srcpath) ) )
    
    return

if __name__ == '__main__' :
    
    makezipdir( sys.argv[1], sys.argv[2] )
    