#include <RcppEigen.h>

#include "nifti1_io.h"
#include "_reg_tools.h"

#include "NiftiImage.h"

using namespace Rcpp;

void NiftiImage::copy (nifti_image * const source)
{
    if (source != NULL)
    {
        size_t dataSize = nifti_get_volsize(source);
        image = nifti_copy_nim_info(source);
        image->data = calloc(1, dataSize);
        memcpy(image->data, source->data, dataSize);
    }
}

void NiftiImage::copy (const NiftiImage &source)
{
    nifti_image *sourceStruct = source;
    copy(sourceStruct);
}

void NiftiImage::copy (const Block &source)
{
    nifti_image *sourceStruct = source.image;
    if (sourceStruct != NULL)
    {
        image = nifti_copy_nim_info(sourceStruct);
        image->dim[0] = source.image->dim[0] - 1;
        image->dim[source.dimension] = 1;
        image->pixdim[source.dimension] = 1.0;
        nifti_update_dims_from_array(image);
        
        size_t blockSize = nifti_get_volsize(image);
        image->data = calloc(1, blockSize);
        memcpy(image->data, static_cast<char*>(source.image->data) + blockSize*source.index, blockSize);
    }
}

// Convert an S4 "nifti" object, as defined in the oro.nifti package, to a "nifti_image" struct
void NiftiImage::initFromNiftiS4 (const RObject &object, const bool copyData)
{
    nifti_1_header header;
    header.sizeof_hdr = 348;
    
    const std::vector<short> dims = object.slot("dim_");
    for (int i=0; i<8; i++)
        header.dim[i] = dims[i];
    
    header.intent_p1 = object.slot("intent_p1");
    header.intent_p2 = object.slot("intent_p2");
    header.intent_p3 = object.slot("intent_p3");
    header.intent_code = object.slot("intent_code");
    
    header.datatype = object.slot("datatype");
    header.bitpix = object.slot("bitpix");
    
    header.slice_start = object.slot("slice_start");
    header.slice_end = object.slot("slice_end");
    header.slice_code = as<int>(object.slot("slice_code"));
    header.slice_duration = object.slot("slice_duration");
    
    const std::vector<float> pixdims = object.slot("pixdim");
    for (int i=0; i<8; i++)
        header.pixdim[i] = pixdims[i];
    header.xyzt_units = as<int>(object.slot("xyzt_units"));
    
    header.vox_offset = object.slot("vox_offset");
    
    header.scl_slope = object.slot("scl_slope");
    header.scl_inter = object.slot("scl_inter");
    header.toffset = object.slot("toffset");
    
    header.cal_max = object.slot("cal_max");
    header.cal_min = object.slot("cal_min");
    header.glmax = header.glmin = 0;
    
    strncpy(header.descrip, as<std::string>(object.slot("descrip")).c_str(), 79);
    header.descrip[79] = '\0';
    strncpy(header.aux_file, as<std::string>(object.slot("aux_file")).c_str(), 23);
    header.aux_file[23] = '\0';
    strncpy(header.intent_name, as<std::string>(object.slot("intent_name")).c_str(), 15);
    header.intent_name[15] = '\0';
    strncpy(header.magic, as<std::string>(object.slot("magic")).c_str(), 3);
    header.magic[3] = '\0';
    
    header.qform_code = object.slot("qform_code");
    header.sform_code = object.slot("sform_code");
    
    header.quatern_b = object.slot("quatern_b");
    header.quatern_c = object.slot("quatern_c");
    header.quatern_d = object.slot("quatern_d");
    header.qoffset_x = object.slot("qoffset_x");
    header.qoffset_y = object.slot("qoffset_y");
    header.qoffset_z = object.slot("qoffset_z");
    
    const std::vector<float> srow_x = object.slot("srow_x");
    const std::vector<float> srow_y = object.slot("srow_y");
    const std::vector<float> srow_z = object.slot("srow_z");
    for (int i=0; i<4; i++)
    {
        header.srow_x[i] = srow_x[i];
        header.srow_y[i] = srow_y[i];
        header.srow_z[i] = srow_z[i];
    }
    
    if (header.datatype == DT_UINT8 || header.datatype == DT_INT16 || header.datatype == DT_INT32 || header.datatype == DT_INT8 || header.datatype == DT_UINT16 || header.datatype == DT_UINT32)
        header.datatype = DT_INT32;
    else if (header.datatype == DT_FLOAT32 || header.datatype == DT_FLOAT64)
        header.datatype = DT_FLOAT64;  // This assumes that sizeof(double) == 8
    else
        throw std::runtime_error("Data type is not supported");
    
    this->image = nifti_convert_nhdr2nim(header, NULL);
    
    const SEXP data = PROTECT(object.slot(".Data"));
    if (!copyData || Rf_length(data) <= 1)
        this->image->data = NULL;
    else
    {
        const size_t dataSize = nifti_get_volsize(this->image);
        this->image->data = calloc(1, dataSize);
        if (header.datatype == DT_INT32)
        {
            IntegerVector intData(data);
            std::copy(intData.begin(), intData.end(), static_cast<int32_t*>(this->image->data));
        }
        else
        {
            DoubleVector doubleData(data);
            std::copy(doubleData.begin(), doubleData.end(), static_cast<double*>(this->image->data));
        }
    }
    UNPROTECT(1);
}

void NiftiImage::initFromArray (const RObject &object)
{
    int dims[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    const std::vector<int> dimVector = object.attr("dim");
    
    const int nDims = std::min(7, int(dimVector.size()));
    dims[0] = nDims;
    for (int i=0; i<nDims; i++)
        dims[i+1] = dimVector[i];
    
    short datatype = DT_UNKNOWN;
    const int sexpType = object.sexp_type();
    if (sexpType == INTSXP || sexpType == LGLSXP)
        datatype = DT_INT32;
    else if (sexpType == REALSXP)
        datatype = DT_FLOAT64;
    else
        throw std::runtime_error("Array elements must be numeric");
    
    this->image = nifti_make_new_nim(dims, datatype, TRUE);
    
    const size_t dataSize = nifti_get_volsize(image);
    if (datatype == DT_INT32)
        memcpy(this->image->data, INTEGER(object), dataSize);
    else
        memcpy(this->image->data, REAL(object), dataSize);
    
    if (object.hasAttribute("pixdim"))
    {
        const std::vector<float> pixdimVector = object.attr("pixdim");
        const int pixdimLength = pixdimVector.size();
        for (int i=0; i<std::min(pixdimLength,nDims); i++)
            this->image->pixdim[i+1] = pixdimVector[i];
    }
}

NiftiImage::NiftiImage (const SEXP object, const bool readData)
{
    RObject imageObject(object);
    
    if (Rf_isNull(object))
        return;
    else if (imageObject.hasAttribute(".nifti_image_ptr"))
    {
        XPtr<NiftiImage> imagePtr(SEXP(imageObject.attr(".nifti_image_ptr")));
        this->image = *imagePtr;
    }
    else if (Rf_isString(object))
    {
        std::string path = as<std::string>(object);
        this->image = nifti_image_read(path.c_str(), readData);
        if (this->image == NULL)
            throw std::runtime_error("Failed to read image");
    }
    else if (imageObject.inherits("nifti"))
        initFromNiftiS4(imageObject, readData);
    else if (imageObject.hasAttribute("dim"))
        initFromArray(imageObject);
    else
        throw std::runtime_error("Cannot convert object of class \"" + as<std::string>(imageObject.attr("class")) + "\" to a nifti_image");
    
    if (this->image != NULL)
        reg_checkAndCorrectDimension(this->image);
}

mat44 NiftiImage::xform (const bool preferQuaternion) const
{
    if (image == NULL)
    {
        mat44 matrix;
        for (int i=0; i<4; i++)
        {
            for (int j=0; j<4; j++)
                matrix.m[i][j] = 0.0;
        }
        return matrix;
    }
    else if (image->qform_code <= 0 && image->sform_code <= 0)
    {
        // No qform or sform so return RAS matrix (NB: other software may assume differently)
        mat44 matrix;
        reg_mat44_eye(&matrix);
        return matrix;
    }
    else if ((preferQuaternion && image->qform_code > 0) || image->sform_code <= 0)
        return image->qto_xyz;
    else
        return image->sto_xyz;
}

NiftiImage allocateMultiregResult (const NiftiImage &source, const NiftiImage &target, const bool forceDouble)
{
    nifti_image *newStruct = nifti_copy_nim_info(target);
    newStruct->dim[0] = source->dim[0];
    newStruct->dim[source.nDims()] = source->dim[source.nDims()];
    newStruct->pixdim[source.nDims()] = source->pixdim[source.nDims()];
    
    if (forceDouble)
    {
        newStruct->datatype = DT_FLOAT64;
        nifti_datatype_sizes(newStruct->datatype, &newStruct->nbyper, NULL);
    }
    
    nifti_update_dims_from_array(newStruct);
    
    size_t dataSize = nifti_get_volsize(newStruct);
    newStruct->data = calloc(1, dataSize);
    
    return NiftiImage(newStruct);
}

template <typename SourceType, typename TargetType>
TargetType convertValue (SourceType value)
{
    return static_cast<TargetType>(value);
}

template <typename SourceType, int SexpType>
RObject imageDataToArray (const nifti_image *source)
{
    if (source == NULL)
        return RObject();
    else
    {
        SourceType *original = static_cast<SourceType *>(source->data);
        Rcpp::Vector<SexpType> array(static_cast<int>(source->nvox));
        
        if (SexpType == INTSXP || SexpType == LGLSXP)
            std::transform(original, original + source->nvox, array.begin(), convertValue<SourceType,int>);
        else if (SexpType == REALSXP)
            std::transform(original, original + source->nvox, array.begin(), convertValue<SourceType,double>);
        else
            throw std::runtime_error("Only numeric arrays can be created");
        
        return array;
    }
}

void finaliseNiftiImage (SEXP xptr)
{
    NiftiImage *object = (NiftiImage *) R_ExternalPtrAddr(xptr);
    object->setPersistence(false);
    delete object;
    R_ClearExternalPtr(xptr);
}

void addAttributes (RObject &object, nifti_image *source, const bool realDim = true)
{
    const int nDims = source->dim[0];
    IntegerVector dim(source->dim+1, source->dim+1+nDims);
    
    if (realDim)
        object.attr("dim") = dim;
    else
        object.attr("imagedim") = dim;
    
    DoubleVector pixdim(source->pixdim+1, source->pixdim+1+nDims);
    object.attr("pixdim") = pixdim;
    
    CharacterVector pixunits(2);
    pixunits[0] = nifti_units_string(source->xyz_units);
    pixunits[1] = nifti_units_string(source->time_units);
    object.attr("pixunits") = pixunits;
    
    NiftiImage *wrappedSource = new NiftiImage(source, true);
    wrappedSource->setPersistence(true);
    XPtr<NiftiImage> xptr(wrappedSource);
    R_RegisterCFinalizerEx(SEXP(xptr), &finaliseNiftiImage, FALSE);
    object.attr(".nifti_image_ptr") = xptr;
}

RObject imageToArray (nifti_image *source)
{
    RObject array;
    
    switch (source->datatype)
    {
        case DT_UINT8:
        array = imageDataToArray<uint8_t,INTSXP>(source);
        break;
        
        case DT_INT16:
        array = imageDataToArray<int16_t,INTSXP>(source);
        break;
        
        case DT_INT32:
        array = imageDataToArray<int32_t,INTSXP>(source);
        break;
        
        case DT_FLOAT32:
        array = imageDataToArray<float,REALSXP>(source);
        break;
        
        case DT_FLOAT64:
        array = imageDataToArray<double,REALSXP>(source);
        break;
        
        case DT_INT8:
        array = imageDataToArray<int8_t,INTSXP>(source);
        break;
        
        case DT_UINT16:
        array = imageDataToArray<uint16_t,INTSXP>(source);
        break;
        
        case DT_UINT32:
        array = imageDataToArray<uint32_t,INTSXP>(source);
        break;
        
        case DT_INT64:
        array = imageDataToArray<int64_t,INTSXP>(source);
        break;
        
        case DT_UINT64:
        array = imageDataToArray<uint64_t,INTSXP>(source);
        break;
        
        default:
        throw std::runtime_error("Unsupported data type (" + std::string(nifti_datatype_string(source->datatype)) + ")");
    }
    
    addAttributes(array, source);
    
    return array;
}

RObject imageToPointer (nifti_image *source, const std::string label)
{
    RObject string = wrap(label);
    addAttributes(string, source, false);
    string.attr("class") = "internalImage";
    return string;
}
