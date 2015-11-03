#include "Array.h"
#include "mpifftw++.h"
#include "utils.h"

using namespace std;
using namespace utils;
using namespace fftwpp;
using namespace Array;

// Number of iterations.
unsigned int N0=10000000;
unsigned int N=0;
int divisor=0; // Test for best block divisor
int alltoall=-1; // Test for best alltoall routine

void init(double *f,
          unsigned int X, unsigned int Y, unsigned int Z,
          unsigned int x0, unsigned int y0, unsigned int z0,
          unsigned int x, unsigned int y, unsigned int z)
{
  unsigned int c=0;
  for(unsigned int i=0; i < x; ++i) {
    unsigned int ii=x0+i;
    for(unsigned int j=0; j < y; j++) {
      unsigned int jj=y0+j;
      for(unsigned int k=0; k < Z; k++) {
        unsigned int kk=k;
        f[c++] = ii + jj + kk;
      }
    }
  }
}

void init(double *f, split3 d)
{
  init(f,d.X,d.Y,d.Z,d.x0,d.y0,d.z0,d.x,d.y,d.z);
}

unsigned int outlimit=3000;

int main(int argc, char* argv[])
{
#ifndef __SSE2__
  fftw::effort |= FFTW_NO_SIMD;
#endif
  int retval=0;

  unsigned int nx=4;
  unsigned int ny=0;
  unsigned int nz=0;

  bool quiet=false;
  bool test=false;
  bool shift=false;
  unsigned int stats=0; // Type of statistics used in timing test.

#ifdef __GNUC__ 
  optind=0;
#endif  
  for (;;) {
    int c = getopt(argc,argv,"S:htN:O:T:a:m:n:s:x:y:z:q");
    if (c == -1) break;
                
    switch (c) {
    case 0:
      break;
    case 'a':
      divisor=atoi(optarg);
      break;
    case 'N':
      N=atoi(optarg);
      break;
    case 'm':
      nx=ny=nz=atoi(optarg);
      break;
    case 's':
      alltoall=atoi(optarg);
      break;
    case 'x':
      nx=atoi(optarg);
      break;
    case 'y':
      ny=atoi(optarg);
      break;
    case 'z':
      nz=atoi(optarg);
      break;
    case 'n':
      N0=atoi(optarg);
      break;
    case 'O':
      shift=atoi(optarg);
      break;
    case 'S':
      stats=atoi(optarg);
      break;
    case 'T':
      fftw::maxthreads=atoi(optarg);
      break;
    case 'q':
      quiet=true;
      break;
    case 't':
      test=true;
      break;
    case 'h':
    default:
      usage(3);
      usageTranspose();
      usageShift();
      exit(1);
    }
  }

  int provided;
  MPI_Init_thread(&argc,&argv,MPI_THREAD_MULTIPLE,&provided);

  if(ny == 0) ny=nx;
  if(nz == 0) nz=nx;

  if(N == 0) {
    N=N0/nx/ny/nz;
    if(N < 10) N=10;
  }
  
  unsigned int nzp=nz/2+1;
  MPIgroup group(MPI_COMM_WORLD,nx,ny);

  if(group.size > 1 && provided < MPI_THREAD_FUNNELED)
    fftw::maxthreads=1;
  
  defaultmpithreads=fftw::maxthreads;
    
  if(group.rank < group.size) {
    bool main=group.rank == 0;
    if(!quiet && main) {
      cout << "Configuration: " 
           << group.size << " nodes X " << fftw::maxthreads 
           << " threads/node" << endl;
      cout << "Using MPI VERSION " << MPI_VERSION << endl;
      cout << "N=" << N << endl;
      cout << "nx=" << nx << ", ny=" << ny << ", nz=" << nz << ", nzp=" << nzp
           << endl;
      cout << "size=" << group.size << endl;
    }

    split3 df(nx,ny,nz,group);
    split3 dg(nx,ny,nzp,group,true);
    
    double *f=doubleAlign(df.n);
    Complex *g=ComplexAlign(dg.n);
    
    rcfft3dMPI rcfft(df,dg,f,g,mpiOptions(divisor,alltoall));

    if(!quiet && group.rank == 0)
      cout << "Initialized after " << seconds() << " seconds." << endl;    
    
    if(test) {
      init(f,df);

      if(!quiet && nx*ny < outlimit) {
        if(main) cout << "\ninput:" << endl;
        show(f,df.x,df.y,df.Z,group.active);
      }

      size_t align=sizeof(Complex);

      array3<double> flocal(nx,ny,nz,align);
      array3<Complex> glocal(nx,ny,nzp,align);
      array3<double> fgathered(nx,ny,nz,align);
      array3<Complex> ggathered(nx,ny,nzp,align);
      
      rcfft3d localForward(nx,ny,nz,fgathered(),ggathered());
      crfft3d localBackward(nx,ny,nz,ggathered(),fgathered());

      gatherxy(f, fgathered(), df, group.active);

      init(flocal(),df.X,df.Y,df.Z,0,0,0,df.X,df.Y,df.Z);
      if(main) {
        if(!quiet) {
          cout << "Gathered input:\n" <<  fgathered << endl;
          cout << "Local input:\n" <<  flocal << endl;
        }
        retval += checkerror(flocal(),fgathered(),df.X*df.Y*df.Z);
      }
      
      if(shift)
	rcfft.Forward0(f,g);
      else
	rcfft.Forward(f,g);
      
      if(main) {
        if(shift)
          localForward.fft0(flocal,glocal);
        else
          localForward.fft(flocal,glocal);
        cout << endl;
      }
        
      if(!quiet) {
        if(main) cout << "Distributed output:" << endl;
        show(g,dg.X,dg.y,dg.z,group.active);
      }
      gatheryz(g,ggathered(),dg,group.active); 

      if(!quiet && main) {
        cout << "Gathered output:\n" <<  ggathered << endl;
        cout << "Local output:\n" <<  glocal << endl;
      }
      
      if(main) {
        retval += checkerror(glocal(),ggathered(),dg.X*dg.Y*dg.Z);
        cout << endl;
      }

      if(shift)
	rcfft.Backward0(g,f);
      else
	rcfft.Backward(g,f);
      rcfft.Normalize(f);

      if(main) {
        if(shift)
          localBackward.fft0Normalized(glocal,flocal);
        else 
          localBackward.fftNormalized(glocal,flocal);
      }
      
      if(!quiet) {
        if(main) cout << "Distributed back to input:" << endl;
        show(f,df.x,df.y,df.Z,group.active);
      }

      gatherxy(f,fgathered(),df,group.active);
      
      if(!quiet && main) {
        cout << "Gathered back to input:\n" <<  fgathered << endl;
        cout << "Local back to input:\n" <<  flocal << endl;
      }
      
      if(main)
        retval += checkerror(flocal(),fgathered(),df.X*df.Y*df.Z);
      
      if(!quiet && group.rank == 0) {
        cout << endl;
        if(retval == 0)
          cout << "pass" << endl;
        else
          cout << "FAIL" << endl;
      }
      
    } else {

      if(main)
        cout << "N=" << N << endl;
      if(N > 0) {
    
        double *T=new double[N];
        for(unsigned int i=0; i < N; ++i) {
          init(f,df);
          seconds();
          rcfft.Forward(f,g);
          rcfft.Backward(g,f);
          rcfft.Normalize(f);
          T[i]=seconds();
        }
        if(!quiet)
          show(f,df.x,df.y,df.Z,group.active);
        
        if(main) timings("FFT timing:",nx,T,N,stats);
        delete[] T;
      }

    }
  
    deleteAlign(g);
    deleteAlign(f);
  }
  
  MPI_Finalize();
  
  return retval;
}
