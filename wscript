top='.'
out='.build'

def options(opt):
	opt.load('compiler_cxx')

def configure(conf):
	conf.load('compiler_cxx')
	conf.env['CFLAGS']=['-O3', '-march=core2', '-mpopcnt', '-msse2', '-mtune=generic', '-Wall', '-DNDEBUG', '-DKENLM_MAX_ORDER=6']
	conf.env['CXXFLAGS']=['-O3', '-march=core2', '-mpopcnt', '-msse2', '-mtune=generic', '-Wall', '-DNDEBUG', '-DKENLM_MAX_ORDER=6']
	conf.env['LINKFLAGS']=['-lpthread']
	conf.env['INCLUDES']=['MurmurHash3','darts-clone']

def build(bld):
	bld.recurse('MurmurHash3')
	bld.recurse('scripts')
	
	projname = 'dalm'
	includes = ['include']
	use = [projname, 'MurmurHash3']

	###### INSTALL HEADERS ######
	headers = ['arpafile.h', 'da.h', 'lm.h', 'logger.h', 'pthread_wrapper.h', 'treefile.h', 'vocabulary.h']
	headers = map(lambda x:'include/%s'%x, headers)
	bld.install_files('${PREFIX}/include', headers)

	###### BUILD DALM LIB ######
	files = ['lm.cpp', 'da.cpp', 'vocabulary.cpp', 'logger.cpp']
	files = map(lambda x: 'src/%s'%x, files)
	
	lib = bld.stlib(source=' '.join(files), target=projname)
	lib.includes = ['include']
	bld.install_files('${PREFIX}/lib/', ['lib%s.a'%projname])

	###### BUILD DUMPER ######
	files = ['dumper.cpp']
	files = map(lambda x: 'src/%s'%x, files)
	
	dumper = bld.program(source=' '.join(files), target='trie_dumper')
	dumper.use = use
	dumper.includes = includes

	###### BUILD DALM BUILDER ######
	files = ['builder.cpp']
	files = map(lambda x: 'src/%s'%x, files)
	builder = bld.program(source=' '.join(files), target=projname+'_builder')
	builder.use = use
	builder.includes = includes

	###### BUILD SAMPLES ######
	files = ['query_sample.cpp']
	files = map(lambda x: 'sample/%s'%x, files)
	builder = bld.program(source=' '.join(files), target='query_sample')
	builder.use = use
	builder.includes = includes
