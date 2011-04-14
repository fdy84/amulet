


if __name__ == '__main__':
    
    import sys
    import os.path
    
    srcpath = sys.argv[1] if len(sys.argv) >= 1 else './'
    verfn = sys.argv[2] if len(sys.argv) >= 2 else None
    
    try :
        
        with open(os.path.join(srcpath,'.svn/entries'),'r') as fp:
            x = fp.read().splitlines()[3]
        
        if verfn :
            with open(verfn,'w') as fp :
                fp.write(x)
        else :
            sys.stdout.write(x)
            
    except IOError, e :
        
        import traceback
        traceback.print_exc()
        
        pass
        