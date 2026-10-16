#include "Layout/scene/layout_anchor_span.h"
#include <math.h>
static double dot(Vec3 a,Vec3 b){return (double)a.x*b.x+(double)a.y*b.y+(double)a.z*b.z;}
bool Layout_AnchorSpanRange(const Layout* l,const LayoutConstraint* c,Vec3 a,Vec3 b,double* minimum,double* maximum){
    LayoutResolvedReference pivot;
    LayoutMeasurementResult measured=Layout_Measure(l,&c->a,&c->b,LAYOUT_MEASURE_PLANAR_ANGLE,c->axis);
    if(!minimum||!maximum||measured.status!=LAYOUT_MEASUREMENT_OK||Layout_ResolveReference(l,&c->a,&pivot)!=LAYOUT_MEASUREMENT_OK)return false;
    Vec3 origin={pivot.point_meters[0],pivot.point_meters[1],pivot.point_meters[2]};
    Vec3 n=Vec3_Scale(c->axis,1/Vec3_Length(c->axis)),q=Vec3_Sub(a,origin),r=Vec3_Sub(b,origin);
    double qa=dot(q,n),ra=dot(r,n);
    Vec3 qp=Vec3_Sub(q,Vec3_Scale(n,qa)),rp=Vec3_Sub(r,Vec3_Scale(n,ra));
    double constant=dot(q,q)+dot(r,r)-2*qa*ra,cosine=-2*dot(qp,rp),sine=-2*dot(qp,Vec3_Cross(n,rp));
    double low=(c->travel_min-measured.value)*.017453292519943295,high=(c->travel_max-measured.value)*.017453292519943295;
    double lo=fmin(constant+cosine*cos(low)+sine*sin(low),constant+cosine*cos(high)+sine*sin(high));
    double hi=fmax(constant+cosine*cos(low)+sine*sin(low),constant+cosine*cos(high)+sine*sin(high));
    double phase=atan2(sine,cosine);
    for(int i=-4;i<=4;++i){double angle=phase+i*3.141592653589793;if(angle<low||angle>high)continue;double value=constant+cosine*cos(angle)+sine*sin(angle);lo=fmin(lo,value);hi=fmax(hi,value);}
    if(!isfinite(lo)||!isfinite(hi)||lo<=1e-12)return false;
    *minimum=sqrt(lo);*maximum=sqrt(hi);return true;
}
