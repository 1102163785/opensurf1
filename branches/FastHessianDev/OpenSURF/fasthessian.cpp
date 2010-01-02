/*********************************************************** 
*  --- OpenSURF ---                                        *
*  This library is distributed under the GNU GPL. Please   *
*  contact chris.evans@irisys.co.uk for more information.  *
*                                                          *
*  C. Evans, Research Into Robust Visual Features,         *
*  MSc University of Bristol, 2008.                        *
*                                                          *
************************************************************/

#include "cv.h"
#include "integral.h"
#include "ipoint.h"
#include "utils.h"

#include <vector>

#include "fasthessian.h"

using namespace std;

//-------------------------------------------------------

class ResponseLayer
{
public:

  ResponseLayer(int width, int height, int filter)
  {
    assert(width > 0 && height > 0);
    
    this->width = width;
    this->height = height;
    this->filter = filter;
    responses = new float[width*height];
    laplacian = new unsigned char[width*height];
  }

  ~ResponseLayer()
  {
    if (responses) delete [] responses;
    if (laplacian) delete [] laplacian;
  }

  int width, height, filter;
  float *responses;
  unsigned char *laplacian;

};

//-------------------------------------------------------
// pre calculated lobe sizes
static const int lobe_cache [] = {3,5,7,9,13,17,25,33,49,65};
static const int lobe_map [] = {0,1,2,3, 1,3,4,5, 3,5,6,7, 5,7,8,9};

//-------------------------------------------------------

//! Constructor without image
FastHessian::FastHessian(std::vector<Ipoint> &ipts, 
                         const int octaves, const int intervals, const int init_sample, 
                         const float thres) 
                         : ipts(ipts), i_width(0), i_height(0)
{
  // Save parameter set
  saveParameters(octaves, intervals, init_sample, thres);
}

//-------------------------------------------------------

//! Constructor with image
FastHessian::FastHessian(IplImage *img, std::vector<Ipoint> &ipts, 
                         const int octaves, const int intervals, const int init_sample, 
                         const float thres) 
                         : ipts(ipts), i_width(0), i_height(0)
{
  // Save parameter set
  saveParameters(octaves, intervals, init_sample, thres);

  // Set the current image
  setIntImage(img);
}

//-------------------------------------------------------

//! Save the parameters
void FastHessian::saveParameters(const int octaves, const int intervals, 
                                 const int init_sample, const float thres)
{
  // Initialise variables with bounds-checked values
  this->octaves = 
    (octaves > 0 && octaves <= 4 ? octaves : OCTAVES);
  this->intervals = 
    (intervals > 0 && intervals <= 4 ? intervals : INTERVALS);
  this->init_sample = 
    (init_sample > 0 && init_sample <= 6 ? init_sample : INIT_SAMPLE);
  this->thres = (thres >= 0 ? thres : THRES);
}


//-------------------------------------------------------

//! Set or re-set the integral image source
void FastHessian::setIntImage(IplImage *img)
{
  // Change the source image
  this->img = img;

  i_height = img->height;
  i_width = img->width;
}

//-------------------------------------------------------

//! Find the image features and write into vector of features
void FastHessian::getIpoints()
{
  // Clear the vector of exisiting ipts
  ipts.clear();
  
  // Get the response layers
  ResponseLayer *bottom, *middle, *top;
  //bottom = &...
  //middle = &...
  //top    = &...




  /*
  for(int o=0; o < octaves; o++) 
  {
    // For each octave double the sampling step of the previous
    int step = init_sample * fRound(pow(2.0f,o));
    int border = border_cache[o];

    // 3x3x3 non-max suppression over whole image
    for ( int i = 1; i < intervals-1; i += 2) {
      for ( int r = border + step; r < i_height - border - step; r += 2*step ) {
        for ( int c = border + step; c < i_width - border - step; c += 2*step ) {

          int i_max = -1, r_max = -1, c_max = -1;
          float max_val = 0;

          // Scan the pixels in this block to find the local extremum.
          for (int ii = i; ii < min(i+2, intervals-1); ii += 1) {
            for (int rr = r; rr < min(r+2*step, i_height - border - step); rr += step) {
              for (int cc = c; cc < min(c+2*step, i_width - border - step); cc += step) {

                float val = getVal(o, ii, cc, rr);

                // record the max value and its location
                if (val > max_val) 
                {
                  max_val = val;
                  i_max = ii;
                  r_max = rr;
                  c_max = cc;
                }
              }
            }
          }

          // Check the block extremum is an extremum across boundaries.
          if (max_val > thres && i_max != -1 && isExtremum(o, i_max, c_max, r_max)) 
          {
            interpolateExtremum(o, i_max, r_max, c_max);
          }
        }
      }
    }  
  }
  */
}

//-------------------------------------------------------

//! Build map of DoH responses
void FastHessian::buildResponseMap()
{
  // clear any existing response layers
  responseMap.clear();

  // Get image attributes
  int w = img->width / init_sample;
  int h = img->height / init_sample;

  // Calculate approximated determinant of hessian values
  responseMap.push_back(new ResponseLayer(w,   h,    9));
  responseMap.push_back(new ResponseLayer(w,   h,   15));
  responseMap.push_back(new ResponseLayer(w,   h,   21));
  responseMap.push_back(new ResponseLayer(w,   h,   27));

  responseMap.push_back(new ResponseLayer(w/2, h/2, 39));
  responseMap.push_back(new ResponseLayer(w/2, h/2, 51));

  responseMap.push_back(new ResponseLayer(w/4, h/4, 75));
  responseMap.push_back(new ResponseLayer(w/4, h/4, 99));
  
  responseMap.push_back(new ResponseLayer(w/8, h/8, 147));
  responseMap.push_back(new ResponseLayer(w/8, h/8, 195));
  
  for (unsigned int i = 0; i < responseMap.size(); ++i)
  {
    buildResponseLayer(responseMap[i]);
  }
}

//-------------------------------------------------------

//! Calculate DoH responses for supplied layer
void FastHessian::buildResponseLayer(ResponseLayer *r)
{
  float *responses = r->responses;  // response storage
  unsigned char *laplacian = r->laplacian; // laplacian sign storage
  int step = i_height / r->height;  // step size for this filter
  int b = (r->filter - 1) / 2 + 1;  // border for this filter
  int l = r->filter / 3;            // lobe for this filter (filter size / 3)
  int w = r->filter;                // filter size
  float inverse_area = 1.f/(w*w);   // normalisation factor
  float Dxx, Dyy, Dxy;

  for(int r = b, index = 0; r < i_height - b; r += step) 
  {
    for(int c = b; c < i_width - b; c += step, ++index) 
    {
      Dxx = BoxIntegral(img, r - l + 1, c - b, 2*l - 1, w)
          - BoxIntegral(img, r - l + 1, c - l / 2, 2*l - 1, l)*3;
      Dyy = BoxIntegral(img, r - b, c - l + 1, w, 2*l - 1)
          - BoxIntegral(img, r - l / 2, c - l + 1, l, 2*l - 1)*3;
      Dxy = + BoxIntegral(img, r - l, c + 1, l, l)
            + BoxIntegral(img, r + 1, c - l, l, l)
            - BoxIntegral(img, r - l, c - l, l, l)
            - BoxIntegral(img, r + 1, c + 1, l, l);

      // Normalise the filter responses with respect to their size
      Dxx *= inverse_area;
      Dyy *= inverse_area;
      Dxy *= inverse_area;

      // Get the determinant of hessian response & laplacian sign
      responses[index] = (Dxx * Dyy - 0.81f * Dxy * Dxy);
      laplacian[index] = (Dxx + Dyy >= 0 ? 1 : 0);
    }
  }
}
  
//-------------------------------------------------------

//! Non Maximal Suppression function
int FastHessian::isExtremum(int octave, int interval, int c, int r)
{
  int step = init_sample * fRound(pow(2.0f,octave));

  // Bounds check
  if (interval - 1 < 0 || interval + 1 > intervals - 1 
    || c - step < 0 || c + step > i_width 
    || r - step < 0 || r + step > i_height)
  {
    return 0;
  }

  float val = getVal(octave,interval, c, r);

  // Check for maximum 
  for(int ii = interval-1; ii <= interval+1; ++ii )
    for(int cc = c - step; cc <= c + step; cc+=step )
      for(int rr = r - step; rr <= r + step; rr+=step ) 
        if (ii != 0 || cc != 0 || rr != 0)
          if(getVal(octave, ii, cc, rr) > val)
            return 0;

  return 1;
}

//-------------------------------------------------------

//! Return the value of the approximated determinant of hessian
inline float FastHessian::getVal(int o, int i, int c, int r)
{
  //return fabs(m_det[(o*intervals+i)*(i_width*i_height) + (r*i_width+c)]);
  return 0;
}

//-------------------------------------------------------

//! Return the sign of the laplacian (trace of the hessian)
inline int FastHessian::getLaplacian(int o, int i, int c, int r)
{
  //float res = (m_det[(o*intervals+i)*(i_width*i_height) + (r*i_width+c)]);

  //return (res >= 0 ? 1 : -1);

  return 0;
}

//-------------------------------------------------------

//! Interpolates a scale-space extremum's location and scale to subpixel
//! accuracy to form an image feature.   
void FastHessian::interpolateExtremum(int octv, int intvl, int r, int c)
{
  double xi = 0, xr = 0, xc = 0;
  int step = init_sample * fRound(pow(2.0f,octv));

  // Get the offsets to the actual location of the extremum
  interpolateStep( octv, intvl, r, c, &xi, &xr, &xc );

  // If point is sufficiently close to the actual extremum
  if( fabs( xi ) < 0.5  &&  fabs( xr ) < 0.5  &&  fabs( xc ) < 0.5 )
  {
    // Create Ipoint and push onto Ipoints vector
    Ipoint ipt;
    ipt.x = static_cast<float>(c + step*xc);
    ipt.y = static_cast<float>(r + step*xr);
    ipt.scale = static_cast<float>((1.2f/9.0f) * (3*(pow(2.0f, octv+1) * (intvl+xi+1)+1)));
    ipt.laplacian = getLaplacian(octv, intvl, c, r);
    ipts.push_back(ipt);
  }
}

//-------------------------------------------------------

//! Performs one step of extremum interpolation. 
void FastHessian::interpolateStep( int octv, int intvl, int r, int c, double* xi, double* xr, double* xc )
{
  CvMat* dD, * H, * H_inv, X;
  double x[3] = { 0 };

  dD = deriv3D( octv, intvl, r, c );
  H = hessian3D( octv, intvl, r, c );
  H_inv = cvCreateMat( 3, 3, CV_64FC1 );
  cvInvert( H, H_inv, CV_SVD );
  cvInitMatHeader( &X, 3, 1, CV_64FC1, x, CV_AUTOSTEP );
  cvGEMM( H_inv, dD, -1, NULL, 0, &X, 0 );

  cvReleaseMat( &dD );
  cvReleaseMat( &H );
  cvReleaseMat( &H_inv );

  *xi = x[2];
  *xr = x[1];
  *xc = x[0];
}

//-------------------------------------------------------

//! Computes the partial derivatives in x, y, and scale of a pixel.
CvMat* FastHessian::deriv3D( int octv, int intvl, int r, int c )
{
  CvMat* dI;
  double dx, dy, ds;
  int step = init_sample * fRound(pow(2.0f,octv));

  dx = ( getVal(octv,intvl, c+step, r ) -
    getVal( octv,intvl, c-step, r ) ) / 2.0;
  dy = ( getVal( octv,intvl, c, r+step ) -
    getVal( octv,intvl, c, r-step ) ) / 2.0;
  ds = ( getVal( octv,intvl+1, c, r ) -
    getVal( octv,intvl-1, c, r ) ) / 2.0;

  dI = cvCreateMat( 3, 1, CV_64FC1 );
  cvmSet( dI, 0, 0, dx );
  cvmSet( dI, 1, 0, dy );
  cvmSet( dI, 2, 0, ds );

  return dI;
}

//-------------------------------------------------------

//! Computes the 3D Hessian matrix for a pixel.
CvMat* FastHessian::hessian3D(int octv, int intvl, int r, int c )
{
  CvMat* H;
  double v, dxx, dyy, dss, dxy, dxs, dys;
  int step = init_sample * fRound(pow(2.0f,octv));

  v = getVal( octv,intvl, c, r );
  dxx = ( getVal( octv,intvl, c+step, r ) + 
    getVal( octv,intvl, c-step, r ) - 2 * v );
  dyy = ( getVal( octv,intvl, c, r+step ) +
    getVal( octv,intvl, c, r-step ) - 2 * v );
  dss = ( getVal( octv,intvl+1, c, r ) +
    getVal( octv,intvl-1, c, r ) - 2 * v );
  dxy = ( getVal( octv,intvl, c+step, r+step ) -
    getVal( octv,intvl, c-step, r+step ) -
    getVal( octv,intvl, c+step, r-step ) +
    getVal( octv,intvl, c-step, r-step ) ) / 4.0;
  dxs = ( getVal( octv,intvl+1, c+step, r ) -
    getVal( octv,intvl+1, c-step, r ) -
    getVal( octv,intvl-1, c+step, r ) +
    getVal( octv,intvl-1, c-step, r ) ) / 4.0;
  dys = ( getVal( octv,intvl+1, c, r+step ) -
    getVal( octv,intvl+1, c, r-step ) -
    getVal( octv,intvl-1, c, r+step ) +
    getVal( octv,intvl-1, c, r-step ) ) / 4.0;

  H = cvCreateMat( 3, 3, CV_64FC1 );
  cvmSet( H, 0, 0, dxx );
  cvmSet( H, 0, 1, dxy );
  cvmSet( H, 0, 2, dxs );
  cvmSet( H, 1, 0, dxy );
  cvmSet( H, 1, 1, dyy );
  cvmSet( H, 1, 2, dys );
  cvmSet( H, 2, 0, dxs );
  cvmSet( H, 2, 1, dys );
  cvmSet( H, 2, 2, dss );

  return H;
}

//-------------------------------------------------------