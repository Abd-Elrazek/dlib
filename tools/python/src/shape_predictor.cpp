// Copyright (C) 2014  Davis E. King (davis@dlib.net)
// License: Boost Software License   See LICENSE.txt for the full license.

#include <dlib/python.h>
#include <dlib/geometry.h>
#include <boost/python/args.hpp>
#include <dlib/image_processing.h>
#include "shape_predictor.h"
#include "conversion.h"

using namespace dlib;
using namespace std;
using namespace boost::python;

// ----------------------------------------------------------------------------------------

full_object_detection run_predictor (
        shape_predictor& predictor,
        object img,
        object rect
)
{
    rectangle box = extract<rectangle>(rect);
    if (is_gray_python_image(img))
    {
        return predictor(numpy_gray_image(img), box);
    }
    else if (is_rgb_python_image(img))
    {
        return predictor(numpy_rgb_image(img), box);
    }
    else
    {
        throw dlib::error("Unsupported image type, must be 8bit gray or RGB image.");
    }
}

// ----------------------------------------------------------------------------------------

rectangle full_obj_det_get_rect (const full_object_detection& detection)
{ return detection.get_rect(); }

unsigned long full_obj_det_num_parts (const full_object_detection& detection)
{ return detection.num_parts(); }

point full_obj_det_part (const full_object_detection& detection, const unsigned long idx)
{
    if (idx < 0 || idx >= detection.num_parts())
    {
        PyErr_SetString(PyExc_IndexError, "Index out of range");
        boost::python::throw_error_already_set();
    }
    return detection.part(idx);
}

std::vector<point> full_obj_det_parts (const full_object_detection& detection)
{
    const unsigned long num_parts = detection.num_parts();
    std::vector<point> parts(num_parts);
    for (unsigned long j = 0; j < num_parts; ++j)
        parts[j] = detection.part(j);
    return parts;
}

boost::shared_ptr<full_object_detection> full_obj_det_init(object& pyrect, object& pyparts)
{
    const unsigned long num_parts = len(pyparts);
    std::vector<point> parts(num_parts);
    rectangle rect = extract<rectangle>(pyrect);

    for (unsigned long j = 0; j < num_parts; ++j)
        parts[j] = extract<point>(pyparts[j]);

    return boost::shared_ptr<full_object_detection>(new full_object_detection(rect, parts));
}

// ----------------------------------------------------------------------------------------

inline void train_shape_predictor_on_images_py (
        const object& pyimages,
        const object& pydetections,
        const std::string& predictor_output_filename,
        const shape_predictor_training_options& options
)
{
    const unsigned long num_images = len(pyimages);
    if (num_images != len(pydetections))
        throw dlib::error("The length of the detections list must match the length of the images list.");

    std::vector<std::vector<full_object_detection> > detections(num_images);
    dlib::array<array2d<rgb_pixel> > images(num_images);
    images_and_nested_params_to_dlib(pyimages, pydetections, images, detections);

    train_shape_predictor_on_images("", images, detections, predictor_output_filename, options);
}


inline double test_shape_predictor_with_images_py (
        const object& pyimages,
        const object& pydetections,
        const object& pyscales,
        const std::string& predictor_filename
)
{
    const unsigned long num_images = len(pyimages);
    const unsigned long num_scales = len(pyscales);
    if (num_images != len(pydetections))
        throw dlib::error("The length of the detections list must match the length of the images list.");

    if (num_scales > 0 && num_scales != num_images)
        throw dlib::error("The length of the scales list must match the length of the detections list.");

    std::vector<std::vector<full_object_detection> > detections(num_images);
    std::vector<std::vector<double> > scales;
    if (num_scales > 0)
        scales.resize(num_scales);
    dlib::array<array2d<rgb_pixel> > images(num_images);

    // Now copy the data into dlib based objects so we can call the trainer.
    for (unsigned long i = 0; i < num_images; ++i)
    {
        const unsigned long num_boxes = len(pydetections[i]);
        for (unsigned long j = 0; j < num_boxes; ++j)
            detections[i].push_back(extract<full_object_detection>(pydetections[i][j]));

        pyimage_to_dlib_image(pyimages[i], images[i]);
        if (num_scales > 0)
        {
            if (num_boxes != len(pyscales[i]))
                throw dlib::error("The length of the scales list must match the length of the detections list.");
            for (unsigned long j = 0; j < num_boxes; ++j)
                scales[i].push_back(extract<double>(pyscales[i][j]));
        }
    }

    return test_shape_predictor_with_images(images, detections, scales, predictor_filename);
}

inline double test_shape_predictor_with_images_no_scales_py (
        const object& pyimages,
        const object& pydetections,
        const std::string& predictor_filename
)
{
    boost::python::list pyscales;
    return test_shape_predictor_with_images_py(pyimages, pydetections, pyscales, predictor_filename);
}

// ----------------------------------------------------------------------------------------

void bind_shape_predictors()
{
    using boost::python::arg;
    {
    typedef full_object_detection type;
    class_<type>("full_object_detection",
    "This object represents the location of an object in an image along with the \
    positions of each of its constituent parts.")
        .def("__init__", make_constructor(&full_obj_det_init),
"requires \n\
    - rect: dlib rectangle \n\
    - parts: list of dlib points")
        .add_property("rect", &full_obj_det_get_rect, "The bounding box of the parts.")
        .add_property("num_parts", &full_obj_det_num_parts, "The number of parts of the object.")
        .def("part", &full_obj_det_part, (arg("idx")), "A single part of the object as a dlib point.")
        .def("parts", &full_obj_det_parts, "A vector of dlib points representing all of the parts.")
        .def_pickle(serialize_pickle<type>());
    }
    {
    typedef shape_predictor_training_options type;
    class_<type>("shape_predictor_training_options",
        "This object is a container for the options to the train_shape_predictor() routine.")
        .add_property("be_verbose", &type::be_verbose,
                                    &type::be_verbose,
                      "If true, train_shape_predictor() will print out a lot of information to stdout while training.")
        .add_property("cascade_depth", &type::cascade_depth,
                                       &type::cascade_depth,
                      "The number of cascades created to train the model with.")
        .add_property("tree_depth", &type::tree_depth,
                                    &type::tree_depth,
                      "The depth of the trees used in each cascade. There are pow(2, get_tree_depth()) leaves in each tree")
        .add_property("num_trees_per_cascade_level", &type::num_trees_per_cascade_level,
                                                     &type::num_trees_per_cascade_level,
                      "The number of trees created for each cascade.")
        .add_property("nu", &type::nu,
                            &type::nu,
                      "The regularization parameter.  Larger values of this parameter \
                       will cause the algorithm to fit the training data better but may also \
                       cause overfitting.")
        .add_property("oversampling_amount", &type::oversampling_amount,
                                             &type::oversampling_amount,
                      "The number of randomly selected initial starting points sampled for each training example")
        .add_property("feature_pool_size", &type::feature_pool_size,
                                           &type::feature_pool_size,
                      "Number of pixels used to generate features for the random trees.")
        .add_property("lambda", &type::lambda,
                                &type::lambda,
                      "Controls how tight the feature sampling should be. Lower values enforce closer features.")
        .add_property("num_test_splits", &type::num_test_splits,
                                         &type::num_test_splits,
                      "Number of split features at each node to sample. The one that gives the best split is chosen.")
        .add_property("feature_pool_region_padding", &type::feature_pool_region_padding,
                                                     &type::feature_pool_region_padding,
                      "Size of region within which to sample features for the feature pool, \
                      e.g a padding of 0.5 would cause the algorithm to sample pixels from a box that was 2x2 pixels")
        .add_property("random_seed", &type::random_seed,
                                     &type::random_seed,
                      "The random seed used by the internal random number generator");
    }
    {
    typedef shape_predictor type;
    class_<type>("shape_predictor",
"This object is a tool that takes in an image region containing some object and \
outputs a set of point locations that define the pose of the object. The classic \
example of this is human face pose prediction, where you take an image of a human \
face as input and are expected to identify the locations of important facial \
landmarks such as the corners of the mouth and eyes, tip of the nose, and so forth.")
        .def("__init__", make_constructor(&load_object_from_file<type>),
"Loads a shape_predictor from a file that contains the output of the \n\
train_shape_predictor() routine.")
        .def("__call__", &run_predictor, (arg("image"), arg("box")),
"requires \n\
    - image is a numpy ndarray containing either an 8bit grayscale or RGB \n\
      image. \n\
    - box is the bounding box to begin the shape prediction inside. \n\
ensures \n\
    - This function runs the shape predictor on the input image and returns \n\
      a single full object detection.")
        .def_pickle(serialize_pickle<type>());
    }
    {
    def("train_shape_predictor", train_shape_predictor_on_images_py,
        (arg("images"), arg("object_detections"), arg("predictor_output_filename"), arg("options")),
"requires \n\
    - options.lambda > 0 \n\
    - options.nu > 0 \n\
    - options.feature_pool_region_padding >= 0 \n\
    - len(images) == len(object_detections) \n\
    - images should be a list of numpy matrices that represent images, either RGB or grayscale. \n\
    - object_detections should be a list of lists of dlib.full_object_detection objects. \
      Each dlib.full_object_detection contains the bounding box and the lists of points that make up the object parts.\n\
ensures \n\
    - Uses the shape_predictor_trainer to train a \n\
      shape_predictor based on the provided labeled images and full object detections.\n\
    - This function will apply a reasonable set of default parameters and \n\
      preprocessing techniques to the training procedure for shape_predictors \n\
      objects.  So the point of this function is to provide you with a very easy \n\
      way to train a basic shape predictor.   \n\
    - The trained shape predictor is serialized to the file predictor_output_filename.");

    def("train_shape_predictor", train_shape_predictor,
        (arg("dataset_filename"), arg("predictor_output_filename"), arg("options")),
"requires \n\
    - options.lambda > 0 \n\
    - options.nu > 0 \n\
    - options.feature_pool_region_padding >= 0 \n\
ensures \n\
    - Uses the shape_predictor_trainer to train a \n\
      shape_predictor based on the labeled images in the XML file \n\
      dataset_filename.  This function assumes the file dataset_filename is in the \n\
      XML format produced by dlib's save_image_dataset_metadata() routine. \n\
    - This function will apply a reasonable set of default parameters and \n\
      preprocessing techniques to the training procedure for shape_predictors \n\
      objects.  So the point of this function is to provide you with a very easy \n\
      way to train a basic shape predictor.   \n\
    - The trained shape predictor is serialized to the file predictor_output_filename.");

    def("test_shape_predictor", test_shape_predictor_py,
        (arg("dataset_filename"), arg("predictor_filename")),
"ensures \n\
    - Loads an image dataset from dataset_filename.  We assume dataset_filename is \n\
      a file using the XML format written by save_image_dataset_metadata(). \n\
    - Loads a shape_predictor from the file predictor_filename.  This means \n\
      predictor_filename should be a file produced by the train_shape_predictor() \n\
      routine. \n\
    - This function tests the predictor against the dataset and returns the \n\
      mean average error of the detector.  In fact, The \n\
      return value of this function is identical to that of dlib's \n\
      shape_predictor_trainer() routine.  Therefore, see the documentation \n\
      for shape_predictor_trainer() for a detailed definition of the mean average error.");

    def("test_shape_predictor", test_shape_predictor_with_images_no_scales_py,
            (arg("images"), arg("detections"), arg("predictor_filename")),
"requires \n\
    - len(images) == len(object_detections) \n\
    - images should be a list of numpy matrices that represent images, either RGB or grayscale. \n\
    - object_detections should be a list of lists of dlib.full_object_detection objects. \
      Each dlib.full_object_detection contains the bounding box and the lists of points that make up the object parts.\n\
 ensures \n\
    - Loads a shape_predictor from the file predictor_filename.  This means \n\
      predictor_filename should be a file produced by the train_shape_predictor()  \n\
      routine. \n\
    - This function tests the predictor against the dataset and returns the \n\
      mean average error of the detector.  In fact, The \n\
      return value of this function is identical to that of dlib's \n\
      shape_predictor_trainer() routine.  Therefore, see the documentation \n\
      for shape_predictor_trainer() for a detailed definition of the mean average error.");


    def("test_shape_predictor", test_shape_predictor_with_images_py,
            (arg("images"), arg("detections"), arg("scales"), arg("predictor_filename")),
"requires \n\
    - len(images) == len(object_detections) \n\
    - len(object_detections) == len(scales) \n\
    - for every sublist in object_detections: len(object_detections[i]) == len(scales[i]) \n\
    - scales is a list of floating point scales that each predicted part location \
      should be divided by. Useful for normalization. \n\
    - images should be a list of numpy matrices that represent images, either RGB or grayscale. \n\
    - object_detections should be a list of lists of dlib.full_object_detection objects. \
      Each dlib.full_object_detection contains the bounding box and the lists of points that make up the object parts.\n\
 ensures \n\
    - Loads a shape_predictor from the file predictor_filename.  This means \n\
      predictor_filename should be a file produced by the train_shape_predictor()  \n\
      routine. \n\
    - This function tests the predictor against the dataset and returns the \n\
      mean average error of the detector.  In fact, The \n\
      return value of this function is identical to that of dlib's \n\
      shape_predictor_trainer() routine.  Therefore, see the documentation \n\
      for shape_predictor_trainer() for a detailed definition of the mean average error.");
    }
}
