


if __name__ == '__main__':
    
    try :
        
        with open('.\svn\entries','r') as fp:
            x = fp.read().splitlines()[3]
        
        with open('ver','w') as fp :
            fp.write(x)
            
    except IOError, e :
        
        with open('ver','w') as fp :
            fp.write('unkown')
        