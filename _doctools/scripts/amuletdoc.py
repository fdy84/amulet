import os.path
import tempfile


class AutoDoc ( object ):
    
    def __init__( self, prjinfo, srcpath ):
        
        if 'project' not in prjinfo :
            raise Exception, 'ProjectName must provied'
        
        self.prjinfo = prjinfo
        self.prjinfo.setdefault('auth','unkown')
        
        self.srcpath = srcpath
        self.fileexts = ['.c','.cpp','.h','.hpp']
        
    @staticmethod
    def findjust( doc ):
        doc = [ d.lstrip() for d in doc ]
        return min( (len(d) - len(d.lstrip())) 
                     for d in doc if len(d) != 0 )
        
    @staticmethod
    def have_autodoc_sign( ln ):
        
        return ( ln.startswith('/*') \
                 and ln.lstrip('/* ').lower().startswith('autodoc')
               )
        
    def parser( self, arg, name ):
        
        print 'file', name
        
        with open( name, 'r' ) as fp :
            
            lns = fp.read().splitlines()
            
            i = 0
            while( i < len(lns) ):
                
                if self.have_autodoc_sign(lns[i]) :
                    
                    print 'autodoc found', name, i
                    
                    j = i
                    while( j < len(lns) and '*/' not in lns[j] ):
                        j += 1
                    
                    if i != j :
                        doc = [ lns[i].split('/*',1)[-1].split('autodoc')[-1] ]\
                              + lns[i+1:j]\
                              + [lns[j].split('*/',1)[0] ]
                    else :
                        doc = lns[i].split('/*',1)[-1].split('autodoc')[-1]\
                                                      .split('*/',1)[0]
                    
                    doc = [ d.expandtabs(4).rstrip() for d in doc ]
                    ljust_len = self.findjust( doc )
                    if ljust_len != 0 :
                        doc = [ ' '*4+d[ljust_len:] for d in doc ]
                    
                    while( len(doc) > 0 and doc[0] == '' ):
                        doc.pop(0)
                    while( len(doc) > 0 and doc[-1] == '' ):
                        doc.pop(-1)
                        
                    doc = '\r\n'.join( doc )
                    
                    i = j
                    while( j < len(lns) and '{' not in lns[j] ):
                        j += 1
                    
                    fdef = lns[i+1:j] + [lns[j].split('{')[0].strip()]
                    fdef = ''.join( [ fe.strip() for fe in fdef ] )
                    
                    arg.append( 
                        '.. c:function:: ' + fdef + '\r\n'\
                        + doc + '\r\n\r\n'
                    )
                    
                    i = j+1
                    
                else :
                    
                    i += 1
        
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
        print 'PATH:', self.srcpath
        os.path.walk( self.srcpath, self.step, funs )
        
        with open( 'template.xrst', 'r' ) as fp :
            formatter = fp.read()
        
        info = self.prjinfo.copy()
        info['Fuctions'] = '\r\n'.join( funs )
        info['DataType'] = ''
        
        with open( outfile, 'w' ) as fp :
            fp.write( formatter % info )

        return
        