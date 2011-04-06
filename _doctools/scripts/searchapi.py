import os.path


class SearchAPI ( object ):
    
    def __init__( self, path ):
        
        self.path = path
        self.fileexts = ['.c','.cpp','.h','.hpp']
        
    def parser( self, name ):
        
        with open( name, 'r' ) as fp :
            a = fp.read()
        
        return
        
    def step( self, arg, dirname, names ):
        
        for n in names :
            
            for ext in self.fileexts :
                if n.endswith(ext):
                    break
            else :
                continue
            
            fn = os.path.join( dirname, n )
            
            if os.path.isfile(fn) :
                
                self.parser(fn)
            
        return
        
    def collects( self ):
        
        funs = []
        
        os.path.walk( self.path, self.step, funs )
        
        return
        