
/* * * * * * * * * * * * * Author's note * * * * * * * * * * * *\
*   _       _   _       _   _       _   _       _     _ _ _ _   *
*  |_|     |_| |_|     |_| |_|_   _|_| |_|     |_|  _|_|_|_|_|  *
*  |_|_ _ _|_| |_|     |_| |_|_|_|_|_| |_|     |_| |_|_ _ _     *
*  |_|_|_|_|_| |_|     |_| |_| |_| |_| |_|     |_|   |_|_|_|_   *
*  |_|     |_| |_|_ _ _|_| |_|     |_| |_|_ _ _|_|  _ _ _ _|_|  *
*  |_|     |_|   |_|_|_|   |_|     |_|   |_|_|_|   |_|_|_|_|    *
*                                                               *
*                     http://www.humus.name                     *
*                                                                *
* This file is a part of the work done by Humus. You are free to   *
* use the code in any way you like, modified, unmodified or copied   *
* into your own work. However, I expect you to respect these points:  *
*  - If you use this file and its contents unmodified, or use a major *
*    part of this file, please credit the author and leave this note. *
*  - For use in anything commercial, please request my approval.     *
*  - Share your work and ideas too as much as you can.             *
*                                                                *
\* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "Vector.h"

half::half(const float x){
  union {
    float floatI;
    unsigned int i;
  };
  floatI = x;

//	unsigned int i = *((unsigned int *) &x);
  int e = ((i >> 23) & 0xFF) - 112;
  int m =  i & 0x007FFFFF;

  sh = (i >> 16) & 0x8000;
  if (e <= 0){
    // Denorm
    m = ((m | 0x00800000) >> (1 - e)) + 0x1000;
    sh |= (m >> 13);
  } else if (e == 143){
    sh |= 0x7C00;
    if (m != 0){
      // NAN
      m >>= 13;
      sh |= m | (m == 0);
    }
  } else {
    m += 0x1000;
    if (m & 0x00800000){
      // Mantissa overflow
      m = 0;
      e++;
    }
    if (e >= 31){
      // Exponent overflow
      sh |= 0x7C00;
    } else {
      sh |= (e << 10) | (m >> 13);
    }
  }
}

half::operator float () const {
  union {
    unsigned int s;
    float result;
  };

  s = (sh & 0x8000) << 16;
  unsigned int e = (sh >> 10) & 0x1F;
  unsigned int m = sh & 0x03FF;

  if (e == 0){
    // +/- 0
    if (m == 0) return result;

    // Denorm
    while ((m & 0x0400) == 0){
      m += m;
      e--;
    }
    e++;
    m &= ~0x0400;
  } else if (e == 31){
    // INF / NAN
    s |= 0x7F800000 | (m << 13);
    return result;
  }

  s |= ((e + 112) << 23) | (m << 13);

  return result;
}

float planeDistance(const vec3 &normal, const float offset, const vec3 &point){
    return point.x * normal.x + point.y * normal.y + point.z * normal.z + offset;
}

float planeDistance(const vec4 &plane, const vec3 &point){
    return point.x * plane.x + point.y * plane.y + point.z * plane.z + plane.w;
}

vec3 rgbeToRGB(unsigned char *rgbe){
  if (rgbe[3]){
    return vec3(rgbe[0], rgbe[1], rgbe[2]) * ldexpf(1.0f, rgbe[3] - (int) (128 + 8));
  } else return vec3(0, 0, 0);
}

unsigned int rgbToRGBE8(const vec3 &rgb){
  float v = max(rgb.x, rgb.y);
  v = max(v, rgb.z);

  if (v < 1e-32f){
    return 0;
  } else {
    int ex;
    float m = frexpf(v, &ex) * 256.0f / v;

    unsigned int r = (unsigned int) (m * rgb.x);
    unsigned int g = (unsigned int) (m * rgb.y);
    unsigned int b = (unsigned int) (m * rgb.z);
    unsigned int e = (unsigned int) (ex + 128);

    return r | (g << 8) | (b << 16) | (e << 24);
  }
}

unsigned int rgbToRGB9E5(const vec3 &rgb){
  float v = max(rgb.x, rgb.y);
  v = max(v, rgb.z);

  if (v < 1.52587890625e-5f){
    return 0;
  } else if (v < 65536){
    int ex;
    float m = frexpf(v, &ex) * 512.0f / v;

    unsigned int r = (unsigned int) (m * rgb.x);
    unsigned int g = (unsigned int) (m * rgb.y);
    unsigned int b = (unsigned int) (m * rgb.z);
    unsigned int e = (unsigned int) (ex + 15);

    return r | (g << 9) | (b << 18) | (e << 27);
  } else {
    unsigned int r = (rgb.x < 65536)? (unsigned int) (rgb.x * (1.0f / 128.0f)) : 0x1FF;
    unsigned int g = (rgb.y < 65536)? (unsigned int) (rgb.y * (1.0f / 128.0f)) : 0x1FF;
    unsigned int b = (rgb.z < 65536)? (unsigned int) (rgb.z * (1.0f / 128.0f)) : 0x1FF;
    unsigned int e = 31;

    return r | (g << 9) | (b << 18) | (e << 27);
  }
}


/* --------------------------------------------------------------------------------- */


mat4 rotateX(const float angle){
  float cosA = cosf(angle), sinA = sinf(angle);
  
  return mat4(
    1, 0,     0,    0,
    0, cosA,  sinA, 0,
    0, -sinA, cosA, 0,
    0, 0,     0,    1);
}

mat4 rotateY(const float angle){
  float cosA = cosf(angle), sinA = sinf(angle);

  return mat4(
    cosA,  0,  sinA, 0,
    0,     1,  0,    0,
    -sinA, 0,  cosA, 0,
    0,     0,  0,    1);
}

mat4 rotateZ(const float angle){
  float cosA = cosf(angle), sinA = sinf(angle);

  return mat4(
    cosA,  sinA, 0, 0,
    -sinA, cosA, 0, 0,
    0,     0,    1, 0,
    0,     0,    0, 1);
}

mat4 rotateXY(const float angleX, const float angleY){
  float cosX = cosf(angleX), sinX = sinf(angleX), 
        cosY = cosf(angleY), sinY = sinf(angleY);

  return mat4(
     cosY,       -sinX * sinY,    cosX * sinY,  0,
     0,           cosX,           sinX,         0,
    -sinY,        -sinX * cosY,   cosX * cosY,  0,
     0,           0,               0,           1);
}

mat4 translate(const vec3 &v){
  return translate(v.x, v.y, v.z);
}

mat4 translate(const float x, const float y, const float z){
  return mat4(1,0,0,0, 0,1,0,0, 0,0,1,0, x,y,z,1);
}

mat4 scale(const float x, const float y, const float z){
  return mat4(x,0,0,0, 0,y,0,0, 0,0,z,0, 0,0,0,1);
}

mat4 perspectiveMatrixX(const float fov, const int width, const int height, const float zNear, const float zFar){
  
  // Left handed perspective, -1..1 in z, FOV for x 
  // (like glm::perspectiveFovLH_NO, but FOV is in x)
  float w = cosf(0.5f * fov) / sinf(0.5f * fov);
  float h = (w * width) / height;

  return mat4(
    w, 0, 0, 0,
    0, h, 0, 0,
    0, 0, (zFar + zNear) / (zFar - zNear), 1,
    0, 0, -(2 * zFar * zNear) / (zFar - zNear), 0);
}


void getProjectionPlanes(const mat4 & a_proj, vec4 a_retPlanes[6])
{
  // Get the projection planes in view space (not normalized yet)
  mat4 trans = transpose(a_proj);
  a_retPlanes[0] = (trans[3] + trans[0]); // Left
  a_retPlanes[1] = (trans[3] - trans[0]); // Right

  a_retPlanes[2] = (trans[3] + trans[1]); // Bottom
  a_retPlanes[3] = (trans[3] - trans[1]); // Top

  a_retPlanes[4] = (trans[3] + trans[2]); // Near
  a_retPlanes[5] = (trans[3] - trans[2]); // Far
}


void planeInvTransform(const mat4 & a_modelView, vec4 * a_retPlanes, int a_planeCount)
{
  // Transform the plane by the model view transpose
  // NOTE: Planes have to be transformed by the inverse transpose of a matrix
  //       Nice reference here: http://www.opengl.org/discussion_boards/showthread.php/159564-Clever-way-to-transform-plane-by-matrix
  //
  //       So for a transform to model space we need to do:
  //            inverse(transpose(inverse(MV)))
  //       This equals : transpose(MV) - see Lemma 5 in http://mathrefresher.blogspot.com.au/2007/06/transpose-of-matrix.html
  mat4 modelViewT = transpose(a_modelView);
  for(int i = 0; i < a_planeCount; i++)
  {
    a_retPlanes[i] = modelViewT * a_retPlanes[i];
  }
}


void planeNormalize(vec4 * a_retPlanes, int a_planeCount)
{
  for(int i = 0; i < a_planeCount; i++)
  {
    float invLength = 1.0f / glm::length(vec3(a_retPlanes[i]));
    a_retPlanes[i] = a_retPlanes[i] * invLength;
  }
}


bool testAABBFrustumPlanes(const vec4 *__restrict a_planes, const vec3& a_boxCenter, const vec3& a_boxExtents)
{
  // Do bit tests?
  // Perhaps update to do this?
  // http://www.gamedev.net/page/resources/_/technical/general-programming/useless-snippet-2-aabbfrustum-test-r3342
  // http://fgiesen.wordpress.com/2010/10/17/view-frustum-culling/
  // http://iquilezles.org/www/articles/frustumcorrect/frustumcorrect.htm
  
  for (uint32_t i = 0; i < 6; i++)
  {
    vec4 curPlane = a_planes[i];
    float distToCenter = planeDistance(curPlane, a_boxCenter);
    float radiusBoxAtPlane = dot(a_boxExtents, glm::abs(vec3(curPlane)));
    if (distToCenter < -radiusBoxAtPlane)
    {
      return false; // Box is entirely behind at least one plane
    }
    //else if (distToCenter <= radiusBoxAtPlane) // If spanned (not entirely infront)
    //{
    //}
  }

  return true;
}


bool testAABBPoint(const vec3& a_boxCenter, const vec3& a_boxExtents, const vec3 & a_point)
{
  return (fabsf(a_point.x - a_boxCenter.x) <= a_boxExtents.x &&
          fabsf(a_point.y - a_boxCenter.y) <= a_boxExtents.y &&
          fabsf(a_point.z - a_boxCenter.z) <= a_boxExtents.z);

  //vec3 min = a_boxCenter - a_boxExtents;
  //vec3 max = a_boxCenter + a_boxExtents;
  //
  //return (a_point.x >= min.x && a_point.x <= max.x && 
  //        a_point.y >= min.y && a_point.y <= max.y && 
  //        a_point.z >= min.z && a_point.z <= max.z);
}


vec3 findAABBClosestPoint(const vec3 & a_point, const vec3& a_boxCenter, const vec3& a_boxExtents)
{
  vec3 retPoint = a_point;

  retPoint = max(retPoint, a_boxCenter - a_boxExtents);
  retPoint = min(retPoint, a_boxCenter + a_boxExtents);

  return retPoint;
}


vec3 findPositionFromTransformMatrix(const mat4 & a_modelView)
{
  // Get the 3 basis vector planes at the camera origin and transform them into model space.
  // Other options here: https://community.khronos.org/t/extracting-camera-position-from-a-modelview-matrix/68031
  //  
  // NOTE: Planes have to be transformed by the inverse transpose of a matrix
  //       Nice reference here: http://www.opengl.org/discussion_boards/showthread.php/159564-Clever-way-to-transform-plane-by-matrix
  //
  //       So for a transform to model space we need to do:
  //            inverse(transpose(inverse(MV)))
  //       This equals : transpose(MV) - see Lemma 5 in http://mathrefresher.blogspot.com.au/2007/06/transpose-of-matrix.html
  //
  // As each plane is simply (1,0,0,0), (0,1,0,0), (0,0,1,0) we can pull the data directly from the transpose matrix.

  const float * a_m = value_ptr(a_modelView);

  // Get plane normals 
  vec3 n1(a_m[0], a_m[4], a_m[8]);
  vec3 n2(a_m[1], a_m[5], a_m[9]);
  vec3 n3(a_m[2], a_m[6], a_m[10]);

  // Get plane distances
  float d1 = a_m[12];
  float d2 = a_m[13];
  float d3 = a_m[14];

  // Get the intersection of these 3 planes
  // http://paulbourke.net/geometry/3planes/
  vec3 n2n3 = cross(n2, n3);
  vec3 n3n1 = cross(n3, n1);
  vec3 n1n2 = cross(n1, n2);

  vec3 top = (n2n3 * d1) + (n3n1 * d2) + (n1n2 * d3);
  float denom = dot(n1, n2n3);
  //if (denom == 0)
  //{
  //  Bad matrix?
  //}

  float denomScale = -1.0f / denom;
  
  return top * denomScale;
}

bool findNearestChaserIntersectionTime(const vec3& chaserPosition, const float chaserSpeed, const vec3& targetPosition, const vec3& targetVelocity, float& retNearestTime)
{
    retNearestTime = 0.0f;
    float targetSpeedSq = dot(targetVelocity, targetVelocity);

    vec3 offsetToTarget = chaserPosition - targetPosition;
    float distanceSq = dot(offsetToTarget, offsetToTarget);
    if (distanceSq == 0.0f)
    {
        retNearestTime = 0.0f;
        return true;
    }

    // Solve by cosine rule and quadratic equation
    // - see https://stackoverflow.com/questions/37250215/intersection-of-two-moving-objects-with-latitude-longitude-coordinates
    float a = (chaserSpeed * chaserSpeed) - targetSpeedSq;
    float b = 2.0f * dot(offsetToTarget, targetVelocity);
    float c = -distanceSq;

    // Solve linear if a == 0
    if (a == 0.0f)
    {
        if (b == 0.0f)
        {
            return false;
        }
        retNearestTime = -c / b;
        return (retNearestTime >= 0.0f);
    }

    float disc = (b * b) - 4.0f * a * c;
    if (disc < 0.0f)
    {
        return false;
    }

    float sqrtDisc = sqrt(disc);
    float first = (-b + sqrtDisc) / (2.0f * a);
    float second = (-b - sqrtDisc) / (2.0f * a);

    if (first < 0.0f &&
        second < 0.0f)
    {
        return false;
    }

    // Return the shortest positive time 
    //- If the second value is positive also, it could be useful as it will indicate an "max intersection time" if the chaser is slower than the target.
    retNearestTime = first;
    if (second >= 0.0f &&
        second < first)
    {
        retNearestTime = second;
    }

    return true;
}

bool findNearestChaserIntersection(const vec3& chaserPosition, const float chaserSpeed, const vec3& targetPosition, const vec3& targetVelocity, vec3& retNearestPos)
{
    float foundTime = 0.0f;
    if (!findNearestChaserIntersectionTime(chaserPosition, chaserSpeed, targetPosition, targetVelocity, foundTime))
    {
        return false;
    }

    retNearestPos = targetPosition + (targetVelocity * foundTime);
    return true;
}

// HOMOGENEOUS CLIPPING
//Taken from https://web.archive.org/web/20110528221654/http://wwwx.cs.unc.edu:80/~sud/courses/236/a5/softgl_homoclip_smooth.cpp
inline bool Inside(const vec4& a_point, CullPlane a_clippingPlane)
{
  switch (a_clippingPlane)
  {
  case CullPlane::Left:   return (a_point.x >= -a_point.w);
  case CullPlane::Right:  return (a_point.x <=  a_point.w);
  case CullPlane::Bottom: return (a_point.y >= -a_point.w);
  case CullPlane::Top:    return (a_point.y <=  a_point.w);
  case CullPlane::Near:   return (a_point.z >= -a_point.w);
  case CullPlane::Far:    return (a_point.z <=  a_point.w);
  }
  return false;
}

inline vec4 Intersect(const vec4& v1, const vec4& v2, CullPlane a_clippingPlane)
{
  // Find the parameter of intersection
  // t = (v1_w-v1_x)/((v2_x - v1_x) - (v2_w - v1_w)) for x=w (RIGHT) plane
  // ... and similar cases
  float t = 0.0f;
  switch (a_clippingPlane)
  {
  case CullPlane::Left:   t = (-v1.w - v1.x) / (v2.x - v1.x + v2.w - v1.w); break;
  case CullPlane::Right:  t = ( v1.w - v1.x) / (v2.x - v1.x - v2.w + v1.w); break;
  case CullPlane::Bottom: t = (-v1.w - v1.y) / (v2.y - v1.y + v2.w - v1.w); break;
  case CullPlane::Top:    t = ( v1.w - v1.y) / (v2.y - v1.y - v2.w + v1.w); break;
  case CullPlane::Near:   t = (-v1.w - v1.z) / (v2.z - v1.z + v2.w - v1.w); break;
  case CullPlane::Far:    t = ( v1.w - v1.z) / (v2.z - v1.z - v2.w + v1.w); break;
  };

  return v1 + ((v2 - v1) * t);
}


void clipPolyToPlane(const std::vector<vec4>& a_inArray, std::vector<vec4 >& a_outArray, CullPlane a_clippingPlane)
{
  a_outArray.resize(0);
  if (a_inArray.size() == 0)
  {
    return;
  }

  const vec4* LastPt = &a_inArray[a_inArray.size() - 1];
  bool bLastIn = Inside(*LastPt, a_clippingPlane);

  for (int32_t i = 0; i < a_inArray.size(); i++)
  {
    const vec4* Pt = &a_inArray[i];

    bool bIn = Inside(*Pt, a_clippingPlane);
    if (bIn != bLastIn)
    {
      a_outArray.push_back(Intersect(*LastPt, *Pt, a_clippingPlane));
    }
    if (bIn)
    {
      a_outArray.push_back(*Pt);
    }

    LastPt = Pt;
    bLastIn = bIn;
  }
}

// Could do a version that does not allocate by passing in triangles, but would do more culling work
bool getPolyScreenArea(std::vector<vec4>& a_inoutArray, std::vector<vec4 >& a_workingArray, uint32_t a_screenWidth, uint32_t a_screenHeight, bool a_clipNearFar, uint32_t& o_startX, uint32_t& o_startY, uint32_t& o_width, uint32_t& o_height)
{
  o_startX = 0;
  o_startY = 0;
  o_width = 0;
  o_height = 0;    
    
  clipPolyToPlane(a_inoutArray, a_workingArray, CullPlane::Left);
  clipPolyToPlane(a_workingArray, a_inoutArray, CullPlane::Right);

  clipPolyToPlane(a_inoutArray, a_workingArray, CullPlane::Bottom);
  clipPolyToPlane(a_workingArray, a_inoutArray, CullPlane::Top);

  if (a_clipNearFar)
  {
    clipPolyToPlane(a_inoutArray, a_workingArray, CullPlane::Near);
    clipPolyToPlane(a_workingArray, a_inoutArray, CullPlane::Far);
  }

  if (a_inoutArray.size() == 0)
  {
    return false;
  }

  // Get the screen space extents of the points
  float minX = FLT_MAX;
  float maxX = -FLT_MAX;
  float minY = FLT_MAX;
  float maxY = -FLT_MAX;
  for (const vec4& inPoint : a_inoutArray)
  {
    // This divide check enables the point positions to OKish when not doing the clip to plane code above.
    // Leaving here just to be sure.
    float Div = inPoint.w;
    if (Div <= 0.0f)
    {
      Div = 0.000001f;
    }

    vec3 processPoint = vec3(inPoint.x, inPoint.y, inPoint.z) / Div;

    processPoint.x = processPoint.x * 0.5f + 0.5f;
    processPoint.y = processPoint.y * -0.5f + 0.5f; // Flipping Y, may not be desired in some orientations

    processPoint.x = clamp(processPoint.x, 0.0f, 1.0f);
    processPoint.y = clamp(processPoint.y, 0.0f, 1.0f);

    processPoint.x *= a_screenWidth;
    processPoint.y *= a_screenHeight;

    minX = min(minX, processPoint.x);
    maxX = max(maxX, processPoint.x);

    minY = min(minY, processPoint.y);
    maxY = max(maxY, processPoint.y);
  }

  // Clamp to pixel half centers (do not include a primitive that does not cross a pixel center)
  int32_t startX = (int32_t)floorf(minX + 0.5f);
  int32_t startY = (int32_t)floorf(minY + 0.5f);

  int32_t endX = (int32_t)ceilf(maxX - 0.5f);
  int32_t endY = (int32_t)ceilf(maxY - 0.5f);

  // Abort if out of bounds or zero width
  if (startX >= endX ||
      startY >= endY)
  {
    return false;
  }

  o_startX = startX;
  o_startY = startY;

  o_width = endX - startX;
  o_height = endY - startY;

  return true;
}


void applyScissorProjection(mat4& a_projection, uint32_t a_screenWidth, uint32_t a_screenHeight, uint32_t a_startX, uint32_t a_startY, uint32_t a_width, uint32_t a_height)
{
  float daX = (float)a_startX / (float)a_screenWidth;
  float daY = (float)a_startY / (float)a_screenHeight;
  float invWidth = (float)a_screenWidth / (float)a_width;
  float invHeight = (float)a_screenHeight / (float)a_height;

  float Tx = (1.0f - (2.0f * daX)) * invWidth - 1.0f;
  float Ty = (-1.0f + (2.0f * daY)) * invHeight + 1.0f;

  mat4 adjustMat(vec4(invWidth, 0, 0, 0),
                 vec4(0, invHeight, 0, 0),
                 vec4(0, 0, 1, 0),
                 vec4(Tx, Ty, 0, 1));

  a_projection = adjustMat * a_projection;
}

