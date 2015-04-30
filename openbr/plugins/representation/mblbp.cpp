#include <opencv2/imgproc/imgproc.hpp>

#include <openbr/plugins/openbr_internal.h>
#include <openbr/core/opencvutils.h>

using namespace cv;

namespace br
{

#define CV_SUM_OFFSETS( p0, p1, p2, p3, rect, step )                      \
    /* (x, y) */                                                          \
    (p0) = (rect).x + (step) * (rect).y;                                  \
    /* (x + w, y) */                                                      \
    (p1) = (rect).x + (rect).width + (step) * (rect).y;                   \
    /* (x + w, y) */                                                      \
    (p2) = (rect).x + (step) * ((rect).y + (rect).height);                \
    /* (x + w, y + h) */                                                  \
    (p3) = (rect).x + (rect).width + (step) * ((rect).y + (rect).height);

class MBLBPRepresentation : public Representation
{
    Q_OBJECT

    Q_PROPERTY(int winWidth READ get_winWidth WRITE set_winWidth RESET reset_winWidth STORED false)
    Q_PROPERTY(int winHeight READ get_winHeight WRITE set_winHeight RESET reset_winHeight STORED false)
    BR_PROPERTY(int, winWidth, 24)
    BR_PROPERTY(int, winHeight, 24)

    void init()
    {
        int offset = winWidth + 1;
        for (int x = 0; x < winWidth; x++ )
            for (int y = 0; y < winHeight; y++ )
                for (int w = 1; w <= winWidth / 3; w++ )
                    for (int h = 1; h <= winHeight / 3; h++ )
                        if ((x+3*w <= winWidth) && (y+3*h <= winHeight) )
                            features.append(Feature(offset, x, y, w, h ) );
    }

    Mat preprocess(const Mat &image) const
    {
        Mat integralImage;
        integral(image, integralImage);
        return integralImage;
    }

    Mat evaluate(const Mat &image, const QList<int> &indices) const
    {
        Mat result(1, indices.size(), CV_32FC1);
        for (int i = 0; i < indices.size(); i++)
            result.at<float>(i) = (float)features[indices[i]].calc(image);
        return result;
    }

    void write(FileStorage &fs, const Mat &featureMap);
    int numFeatures() const { return features.size(); }
    Size windowSize() const { return Size(winWidth + 1, winHeight + 1); }

    struct Feature
    {
        Feature() { rect = Rect(0, 0, 0, 0); }
        Feature( int offset, int x, int y, int _block_w, int _block_h  );
        uchar calc(const Mat &img) const;
        void write( FileStorage &fs ) const { fs << "rect" << "[:" << rect.x << rect.y << rect.width << rect.height << "]"; }

        Rect rect;
        int p[16];
    };
    QList<Feature> features;
};

BR_REGISTER(Representation, MBLBPRepresentation)

void MBLBPRepresentation::write(FileStorage &fs, const Mat &featureMap)
{
    fs << "features" << "[";
    const Mat_<int>& featureMap_ = (const Mat_<int>&)featureMap;
    for ( int fi = 0; fi < featureMap.cols; fi++ )
        if ( featureMap_(0, fi) >= 0 )
        {
            fs << "{";
            features[fi].write( fs );
            fs << "}";
        }
    fs << "]";
}

MBLBPRepresentation::Feature::Feature( int offset, int x, int y, int _blockWidth, int _blockHeight )
{
    Rect tr = rect = cvRect(x, y, _blockWidth, _blockHeight);
    CV_SUM_OFFSETS( p[0], p[1], p[4], p[5], tr, offset )
    tr.x += 2*rect.width;
    CV_SUM_OFFSETS( p[2], p[3], p[6], p[7], tr, offset )
    tr.y +=2*rect.height;
    CV_SUM_OFFSETS( p[10], p[11], p[14], p[15], tr, offset )
    tr.x -= 2*rect.width;
    CV_SUM_OFFSETS( p[8], p[9], p[12], p[13], tr, offset )
}

inline uchar MBLBPRepresentation::Feature::calc(const Mat &img) const
{
    const int* psum = img.ptr<int>();
    int cval = psum[p[5]] - psum[p[6]] - psum[p[9]] + psum[p[10]];

    return (uchar)((psum[p[0]] - psum[p[1]] - psum[p[4]] + psum[p[5]] >= cval ? 128 : 0) |   // 0
        (psum[p[1]] - psum[p[2]] - psum[p[5]] + psum[p[6]] >= cval ? 64 : 0) |    // 1
        (psum[p[2]] - psum[p[3]] - psum[p[6]] + psum[p[7]] >= cval ? 32 : 0) |    // 2
        (psum[p[6]] - psum[p[7]] - psum[p[10]] + psum[p[11]] >= cval ? 16 : 0) |  // 5
        (psum[p[10]] - psum[p[11]] - psum[p[14]] + psum[p[15]] >= cval ? 8 : 0) | // 8
        (psum[p[9]] - psum[p[10]] - psum[p[13]] + psum[p[14]] >= cval ? 4 : 0) |  // 7
        (psum[p[8]] - psum[p[9]] - psum[p[12]] + psum[p[13]] >= cval ? 2 : 0) |   // 6
        (psum[p[4]] - psum[p[5]] - psum[p[8]] + psum[p[9]] >= cval ? 1 : 0));     // 3
}

} // namespace br

#include "representation/mblbp.moc"