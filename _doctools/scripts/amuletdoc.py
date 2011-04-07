import os.path
import tempfile


class AutoDoc ( object ):
    
    def __init__( self, prjinfo, srcpath ):
        
        if 'Project' not in prjinfo :
            raise Exception, 'ProjectName must provied'
        
        self.prjinfo = prjinfo
        self.prjinfo.setdefault('Auth','Unkown')
        
        self.srcpath = srcpath
        self.fileexts = ['.c','.cpp','.h','.hpp']
        
    @staticmethod
    def findjust( lns ):
        doc = [ d.lstrip() for d in doc ]
        return min( (len(d) - len(d.lstrip())) 
                     for d in doc if len(d) != 0 )
        
    def parser( self, arg, name ):
        
        with open( name, 'r' ) as fp :
            
            lns = fp.read().splitlines()
            
            i = 0
            while( i < len(lns) ):
                
                if lns[i].startswith('/*') \
                                and a.strip('/* ').lower().startswith('autodoc ') :
                    
                    j = i
                    while( j < len(lns) and '*/' not in lns[j] ):
                        pass
                    
                    if i != j :
                        doc = [ lns[i].split('/*',1)[-1].split('autodoc ')[-1] ]\
                              + lns[i+1:j-1]
                              + [lns[j].split('*/',1)[0] ]
                    else :
                        doc = lns[i].split('/*',1)[-1].split('autodoc ')[-1]\
                                                      .split('*/',1)[0]
                    
                    doc = [ d.expandtabs(4).rstrip() for d in doc ]
                    ljust_len = self.findjust( doc )
                    if ljust_len != 0 :
                        doc = [ d[ljust_len:].ljust(4) for d in doc ]
                    
                    while( len(doc) > 0 and doc[0] == '' ):
                        doc.pop(0)
                    while( len(doc) > 0 and doc[-1] == '' ):
                        doc.pop(-1)
                        
                    doc = '\r\n'.join( for d in doc )
                    
                    i = j
                    while( j < len(lns) and '{' in lns[j] ):
                        pass
                    
                    fdef = lns[i:j-1] + [lns[j].split('{')[0]]
                    fdef = ''.join( [ fe.strip() for fe in fdef ] )
                    
                    arg.append( 
                        '.. c:function::' + fdef + '\r\n'\
                        + doc + '\r\n\r\n'
                    )
                    
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
                
                self.parser( arg, fn )
            
        return
        
    def compile( self, outfile ):
        
        funs = []
        
        os.path.walk( self.srcpath, self.step, funs )
        
        with open( 'template.xrst', 'r' ) as fp :
            formatter = fp.read()
        
        info = self.prjinfo.copy()
        info['Fuctions'] = '\r\n'.join( funs )
        info['DataType'] = ''
        
        with open( outfile, 'w' ) as fp :
            fp.write( formatter % info )

        return
        