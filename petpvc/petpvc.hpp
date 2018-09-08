#include <iostream>
#include <memory>

#include "itkAddImageFilter.h"
#include "itkBinaryThresholdImageFilter.h"
#include "itkDivideImageFilter.h"
#include "itkExtractImageFilter.h"
#include "itkImage.h"
#include "itkImageDuplicator.h"
#include "itkImageFileReader.h"
#include "itkImageFileWriter.h"
#include "itkLabelStatisticsImageFilter.h"
#include "itkMultiplyImageFilter.h"
#include "itkSubtractImageFilter.h"

namespace petpvc {

typedef itk::Image<float, 3> ImageType3D;
typedef itk::Image<float, 4> ImageType4D;

typedef itk::ExtractImageFilter<ImageType4D, ImageType3D> Extract4DFilterType;

enum class EImageType { E3DImage, E4DImage, EUnknown };

template<typename TImageType>
void ReadFile(const std::string &filename, typename TImageType::Pointer image)
{
  typedef itk::ImageFileReader<TImageType> ReaderType;
  typename ReaderType::Pointer reader = ReaderType::New();

  reader->SetFileName(filename);
  reader->Update();

  image->Graft(reader->GetOutput());
}

template<typename TImageType>
void WriteFile(typename TImageType::Pointer image, const std::string &filename)
{
  typedef typename itk::ImageFileWriter<TImageType> WriterType;
  typename WriterType::Pointer writer = WriterType::New();
  writer->SetInput(image);
  writer->SetFileName(filename);
  writer->Update();
}

template<typename TImageType=ImageType3D>
void CreateBlankImageFromExample(const typename TImageType::Pointer input, typename TImageType::Pointer &output)
{
  typedef itk::ImageDuplicator< TImageType > DuplicatorType;
  typedef typename TImageType::PixelType PixelType;
  typename DuplicatorType::Pointer duplicator = DuplicatorType::New();
  duplicator->SetInputImage(input);
  duplicator->Update();
  typename TImageType::Pointer clonedImage = duplicator->GetOutput();

  clonedImage->FillBuffer( itk::NumericTraits< PixelType >::Zero );

  output->Graft(clonedImage);

}

class PETPVCImageObject {

public:
  PETPVCImageObject(){};
  explicit PETPVCImageObject(const std::string &filename);
  virtual void getVolume( const int n, ImageType3D::Pointer &vol )=0;
  virtual int getNoOfVolumes()=0;
  virtual ~PETPVCImageObject(){};

protected:
  std::string _inFilename;
};

PETPVCImageObject::PETPVCImageObject(const std::string &filename){
  _inFilename = filename;
  std::cout << "Base ctor" << std::endl;
};

class ImageIn4D : public PETPVCImageObject {
public:
  ImageIn4D(const std::string &filename);

  void getVolume( const int n, ImageType3D::Pointer &vol );
  int getNoOfVolumes(){ return _internalImage->GetLargestPossibleRegion().GetSize(3); };
protected:
  ImageType4D::Pointer _internalImage;
};

class ImageIn3D : public PETPVCImageObject {
public:
  ImageIn3D(){};
  ImageIn3D(const std::string &filename);

  void getVolume( const int n, ImageType3D::Pointer &vol );
  int getNoOfVolumes(){ return 1; };

protected:
  ImageType3D::Pointer _internalImage;
};

class MaskIn3D : public ImageIn3D {
public:
  MaskIn3D(const std::string &filename);

  //For getting information about the mask labels
  typedef itk::LabelStatisticsImageFilter<ImageType3D, ImageType3D> LabelStatisticsFilterType;
  typedef typename LabelStatisticsFilterType::ValidLabelValuesContainerType ValidLabelValuesType;
  typedef typename LabelStatisticsFilterType::LabelPixelType LabelPixelType;

  typedef itk::BinaryThresholdImageFilter<ImageType3D, ImageType3D> BinaryThresholdImageFilterType;

  void getRegion( const int n, ImageType3D::Pointer &reg );
};


ImageIn3D::ImageIn3D(const std::string &filename){
  _inFilename = filename;
  _internalImage = ImageType3D::New();
  ReadFile<ImageType3D>( _inFilename, _internalImage );
}

ImageIn4D::ImageIn4D(const std::string &filename){
  _inFilename = filename;
  _internalImage = ImageType4D::New();
  ReadFile<ImageType4D>( _inFilename, _internalImage );
}

MaskIn3D::MaskIn3D(const std::string &filename){
  //TODO implement MaskIn3D ctor

  //Get labels

  //Put labels into array/vector
  //Set up binary filter.
}

void ImageIn3D::getVolume( const int n, ImageType3D::Pointer &vol ) {
  std::cout << "3D image: " << _inFilename << std::endl;

  if ( n != 1) {
    std::cerr << "Requested region " << n << " out of range!" << std::endl;
    throw false;
  }
  vol->Graft(_internalImage);
}

void ImageIn4D::getVolume( const int n, ImageType3D::Pointer &vol ) {
  std::cout << "4D image: " << _inFilename << std::endl;


  typename ImageType4D::IndexType desiredStart;
  desiredStart.Fill(0);

  typename ImageType4D::SizeType desiredSize =
      _internalImage->GetLargestPossibleRegion().GetSize();

  if ( n-1 < 0) {
    std::cerr << "Requested region " << n << " out of range!" << std::endl;
    throw false;
  }

  desiredStart[3] = n - 1;
  desiredSize[3] = 0;

  typename Extract4DFilterType::Pointer extractFilter =
      Extract4DFilterType::New();

  extractFilter->SetExtractionRegion(
      typename ImageType4D::RegionType(desiredStart, desiredSize));
  extractFilter->SetInput(_internalImage);
  extractFilter->SetDirectionCollapseToIdentity();
  extractFilter->Update();

  vol->Graft(extractFilter->GetOutput());

};

void MaskIn3D::getRegion( const int n, ImageType3D::Pointer &reg ){
  //TODO implement MaskIn3D getRegion
}

std::unique_ptr<PETPVCImageObject> CreateImage(EImageType e, const std::string &filename, bool isMask=false){
  if ( (e == EImageType::E3DImage) && (!isMask) )
    return std::unique_ptr<PETPVCImageObject>(new ImageIn3D(filename));
  if ( (e == EImageType::E4DImage) && (!isMask) )
    return std::unique_ptr<PETPVCImageObject>(new ImageIn4D(filename));
  if ( (e == EImageType::E3DImage) && (isMask) )
    return std::unique_ptr<PETPVCImageObject>(new MaskIn3D(filename));

  return nullptr;
}

static bool GetImageIO(const std::string &filename, itk::ImageIOBase::Pointer &imageIO) {

  imageIO = itk::ImageIOFactory::CreateImageIO(
      filename.c_str(), itk::ImageIOFactory::ReadMode);

  if (!imageIO) {
    std::cerr << "Could not CreateImageIO for: " << filename << std::endl;
    return false;
  }

  imageIO->SetFileName(filename);
  imageIO->ReadImageInformation();

  return true;
}

static EImageType GetImageType(const std::string &filename){

  itk::ImageIOBase::Pointer imageIO;
  petpvc::GetImageIO(filename, imageIO);

  const size_t numOfDims = imageIO->GetNumberOfDimensions();

  std::cout << "numDimensions: " << numOfDims << std::endl;

  switch ( numOfDims ){
    case 3 : return EImageType::E3DImage; break;
    case 4 : return EImageType::E4DImage; break;
  }

  return EImageType::EUnknown;
}

template<typename TImageType=ImageType3D>
void Add(const typename TImageType::Pointer a,
         const typename TImageType::Pointer b,
         typename TImageType::Pointer outputImage)
{
  typedef typename itk::AddImageFilter<TImageType> FilterType;
  typename FilterType::Pointer add = FilterType::New();
  add->SetInput1(a);
  add->SetInput2(b);
  add->Update();

  outputImage->Graft(add->GetOutput());
}

template<typename TImageType=ImageType3D>
void Subtract(const typename TImageType::Pointer a,
              const typename TImageType::Pointer b,
              typename TImageType::Pointer outputImage)
{
  typedef typename itk::SubtractImageFilter<TImageType> FilterType;
  typename FilterType::Pointer sub = FilterType::New();
  sub->SetInput1(a);
  sub->SetInput2(b);
  sub->Update();

  outputImage->Graft(sub->GetOutput());
}

template<typename TImageType=ImageType3D>
void Multiply(const typename TImageType::Pointer a,
              const typename TImageType::Pointer b,
              typename TImageType::Pointer outputImage)
{
  typedef typename itk::MultiplyImageFilter<TImageType> FilterType;
  typename FilterType::Pointer multiply = FilterType::New();
  multiply->SetInput1(a);
  multiply->SetInput2(b);
  multiply->Update();

  outputImage->Graft(multiply->GetOutput());
}

template<typename TImageType=ImageType3D>
void Divide(const typename TImageType::Pointer a,
            const typename TImageType::Pointer b,
            typename TImageType::Pointer outputImage)
{
  typedef typename itk::MultiplyImageFilter<TImageType> FilterType;
  typename FilterType::Pointer divide = FilterType::New();
  divide->SetInput1(a);
  divide->SetInput2(b);
  divide->Update();

  outputImage->Graft(divide->GetOutput());
}

}