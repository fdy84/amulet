
import ConfigParser
import os
import os.path
import shutil

import amuletdoc

class DocTool( object ):
    
    newlines = '\n'
    
    def __init__ ( self, srcpath='../../', buildpath='../build' ) :
        
        config = ConfigParser.ConfigParser()
        config.read('../etc/amuletdoc.conf')
        
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
            
        try :
            os.makedirs( os.path.join(self.buildpath,'_static') )
        except OSError, e :
            pass
        
        for prj in self.prjs :
            
            doc = amuletdoc.AutoDoc( prj, self.srcpath )
            
            doc.compile( os.path.join(self.buildpath,prj['project'])+'.rst' )
            
        prjidxs = [ prj['project']+'.rst' for prj in self.prjs ]
        prjidxs = self.newlines.join( ' '*4+prj for prj in prjidxs )

        with open( 'index.xrst', 'rU' ) as fp :
            formatter = fp.read()
        
        with open( os.path.join(self.buildpath,'index.rst'), 'w' ) as fp :
            fp.write( formatter % {'projects':prjidxs} )
        
        for name in os.listdir('./'):
            if name.endswith('._am') :
                shutil.copy( name, os.path.join( self.buildpath, name[:-4] ) )
        
        return


if __name__ == '__main__':
    
    dt = DocTool()
    dt.compile()
    print 'OK'
    
    