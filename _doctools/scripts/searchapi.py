import os.path


class SearchAPI ( object ):
    
    def __init__( self, path ):
        
        self.path = path
        self.fileexts = ['.c','.cpp','.h','.hpp']
        
    def parser( self, name ):
        
        with open( name, 'r' ) as fp :
            
            lns = fp.read().splitlines()
            
            i = 0
            while( i < len(lns) ):
                
                if lns[i].startswith('/*') \
                                and a.strip('/*').lower() = 'autodoc' :
                    
                    j = i
                    while( j < len(lns) and '*/' not in lns[j] ):
                        pass
                    
                    if i != j :
                        doc = [ lns[i].split('/*',1)[-1] ]\
                              + lns[i+1:j-1]
                              + [lns[j].split('*/',1)[0] ]
                    else :
                        doc = lns[i].split('/*',1)[-1].split('*/',1)[0]
            
                    doc = '' + '\r\n'.join(doc)
                    
                    i = j
                    while( j < len(lns) and '{' in lns[j] ):
                        pass
                    
                    fdef = lns[i:j-1] + [lns[j].split('{')[0]]
                    fdef = ''.join(fdef)
                    
                    i = j+1
                    
        
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
        