
import ConfigParser
import os.path

import searchapi

class DocTool( object ):
    
    def __init__ ( self, buildpath=',/build' ) :
        
        config = ConfigParser.ConfigParser()
        config.read('config.conf')
        
        self.prjs = [ ( [('project',s)]\
                        + [ (o,config.get(s,o)) for o in config.options(s) ] )
                      for s in config.sections ]
        
        self.buildpath = buildpath
        
    def compile( self ):
        
        for prj in prjs :
            
            doc = searchapi.SearchAPI( 
                    prj, os.path.join('../../',prj.get('src',prj['project'])) )
            
            doc.complie( os.path.join(buildpath,prj['project'])+'.rst' )
            
        projectindexs = [ prj['project']+'.rst' for prj in prjs ]

        with open( 'index.xrst', 'r' ) as fp :
            formatter = fp.read()
        
        with open( 'index.rst', 'w' ) as fp :
            fp.write( formatter % {'projects':'projectindexs'} )
        
        return
