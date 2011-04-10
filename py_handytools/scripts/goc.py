
# super getopt compiler

# example :
#
#  cmd [-D/--dir path ..] [-h/--host ip[:port] ] [-p port] [-x [a ...] ] [-H/--help] [-v/--version]
#    api ( path, host, help, version ) :
#      path = path
#      host.ip = ip
#      host.port = int(port)
#      help = @help
#      version = @version
#    doc :
#      dir  :  some text
#      host :
#
