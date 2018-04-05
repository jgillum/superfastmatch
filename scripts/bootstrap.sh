rm -Rf external
mkdir external
unamestr=`uname`
if [[ "$unamestr" == 'Linux' ]]; then
        curl -L http://download.savannah.gnu.org/releases/libunwind/libunwind-1.0.1.tar.gz -o external/libunwind-1.0.1.tar.gz
fi
curl https://src.fedoraproject.org/repo/pkgs/ctemplate/ctemplate-2.2.tar.gz/1de89d9073f473c1e31862c4581636f3/ctemplate-2.2.tar.gz -o external/ctemplate-2.2.tar.gz
curl -L https://launchpad.net/ubuntu/+archive/primary/+files/google-perftools_2.0.orig.tar.gz -o external/google-perftools-2.0.tar.gz
curl -L https://github.com/gflags/gflags/archive/v2.2.1.tar.gz -o external/gflags-2.0.tar.gz
curl http://fallabs.com/kyotocabinet/pkg/kyotocabinet-1.2.76.tar.gz -o external/kyotocabinet-1.2.76.tar.gz
curl http://fallabs.com/kyototycoon/pkg/kyototycoon-0.9.56.tar.gz -o external/kyototycoon-0.9.56.tar.gz
curl http://www.oberhumer.com/opensource/lzo/download/lzo-2.06.tar.gz -o external/lzo-2.06.tar.gz
curl -L https://github.com/sparsehash/sparsehash/archive/sparsehash-2.0.3.tar.gz -o external/sparsehash-2.0.2.tar.gz
git clone https://github.com/google/re2.git external/re2

cd external && for i in *.tar.gz; do tar xzvf $i; done
if [[ "$unamestr" == 'Linux' ]]; then
        cd libunwind* && CFLAGS=-U_FORTIFY_SOURCE ./configure && make && sudo make install && cd ..
fi
cd ctemplate* && ./configure && make && sudo make install && cd ..
cd google-perftools* && ./configure && make && sudo make install  && cd ..
cd gflags* && mkdir build && cd build && cmake .. && make && make install && cd .. && cd ..
cd kyotocabinet* && ./configure && make && sudo make install && cd ..
cd kyototycoon* && sed -i 's/\= getpid()/\= kyotocabinet::getpid()/g' ktdbext.h && ./configure && make && sudo make install && cd ..
cd sparsehash-sparsehash* && ./configure && make && sudo make install && cd ..
cd lzo-2.06 && ./configure && make && sudo make install && cd ..
cd re2 && git apply ../../scripts/re2.diff && make && sudo make install && cd ..

cd ..
sudo ldconfig
