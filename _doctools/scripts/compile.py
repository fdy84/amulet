
import ConfigParser
import os.path

import amuletdoc

class DocTool( object ):
    
    def __init__ ( self, srcpath='../../', buildpath='../build' ) :
        
        config = ConfigParser.ConfigParser()
        config.read('config.conf')
        
        self.prjs = [ ( [('project',s)]\
                        + [ (o,config.get(s,o)) for o in config.options(s) ] )
                      for s in config.sections() ]
        self.prjs = [ dict(prj) for prj in self.prjs ]
        
        self.buildpath = os.path.abspath(buildpath)
        self.srcpath = os.path.abspath(srcpath)
        
    def compile( self ):
        
        try :
            os.makedirs( self.buildpath )
        except OSError, e :
            pass
        
        for prj in self.prjs :
            
            prjpath = os.path.join( self.srcpath, 
                                    prj.get( 'src', prj['project'] ) )
            doc = amuletdoc.AutoDoc( prj, prjpath )
            
            doc.compile( os.path.join(self.buildpath,prj['project'])+'.rst' )
            
        prjidxs = [ prj['project']+'.rst' for prj in self.prjs ]
        prjidxs = '\r\n'.join( ' '*4+prj for prj in prjidxs )

        with open( 'index.xrst', 'r' ) as fp :
            formatter = fp.read()
        
        with open( os.path.join(self.buildpath,'index.rst'), 'w' ) as fp :
            fp.write( formatter % {'projects':prjidxs} )
        
        return


if __name__ == '__main__':
    
    dt = DocTool()
    dt.compile()
    