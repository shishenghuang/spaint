/**
 * spaint: ObjectSegmentationComponent.cpp
 * Copyright (c) Torr Vision Group, University of Oxford, 2016. All rights reserved.
 */

#include "pipelinecomponents/ObjectSegmentationComponent.h"

#include <boost/serialization/extended_type_info.hpp>
#include <boost/serialization/singleton.hpp>
#include <boost/serialization/shared_ptr.hpp>

#include <orx/persistence/ImagePersister.h>

#include "segmentation/SegmentationUtil.h"

#if WITH_ARRAYFIRE && WITH_OPENCV
#include "segmentation/BackgroundSubtractingObjectSegmenter.h"
#endif

using namespace itmx;
using namespace orx;
using namespace tvgutil;

namespace spaint {

//#################### CONSTRUCTORS ####################

ObjectSegmentationComponent::ObjectSegmentationComponent(const ObjectSegmentationContext_Ptr& context, const std::string& sceneID, const SingleRGBDImagePipe_Ptr& outputPipe)
: m_context(context), m_outputEnabled(false), m_outputPipe(outputPipe), m_sceneID(sceneID)
{}

//#################### PUBLIC MEMBER FUNCTIONS ####################

void ObjectSegmentationComponent::reset_segmenter()
{
  const Segmenter_Ptr& segmenter = get_segmenter();
  if(segmenter) segmenter->reset();
}

void ObjectSegmentationComponent::run_segmentation(const VoxelRenderState_CPtr& renderState)
{
  // Gets the current segmenter. If there isn't one, early out.
  const Segmenter_Ptr& segmenter = get_segmenter();
  if(!segmenter) return;

  // Segment the current input images to obtain a mask for the target.
  const SLAMState_Ptr& slamState = m_context->get_slam_state(m_sceneID);
  ORUCharImage_CPtr targetMask = segmenter->segment(slamState->get_pose(), renderState);

  // If there's a target mask, use its inverse to mask the camera input for tracking purposes. If not, early out.
  if(targetMask)
  {
    slamState->set_input_mask(SegmentationUtil::invert_mask(targetMask));
  }
  else
  {
    slamState->set_input_mask(ORUCharImage_Ptr());
    m_context->set_segmentation_image(m_sceneID, ORUChar4Image_Ptr());
    return;
  }

  // Make masked versions of the depth and RGB inputs.
  View_CPtr view = slamState->get_view();
  ORUChar4Image_Ptr colouredDepthInput(new ORUChar4Image(view->depth->noDims, true, false));
  m_context->get_visualisation_generator()->get_depth_input(colouredDepthInput, view);
  ORShortImage_CPtr depthInput = slamState->get_input_raw_depth_image_copy();
  ORUChar4Image_CPtr rgbInput(slamState->get_view()->rgb, boost::serialization::null_deleter());

  ORUChar4Image_CPtr colouredDepthMasked = SegmentationUtil::apply_mask(targetMask, colouredDepthInput, Vector4u((uchar)0));
  ORShortImage_CPtr depthMasked = SegmentationUtil::apply_mask(targetMask, depthInput, 0);
  ORUChar4Image_CPtr rgbMasked = SegmentationUtil::apply_mask(targetMask, rgbInput, Vector4u((uchar)0));

  // If output is enabled, write the masked images to the output pipe.
  if(m_outputEnabled && m_outputPipe) m_outputPipe->set_images(rgbMasked, depthMasked);

  // If we're currently saving a segmentation video, save the original and masked versions of the depth and colour inputs to disk.
  boost::optional<SequentialPathGenerator>& segmentationPathGenerator = m_context->get_segmentation_path_generator();
  if(segmentationPathGenerator)
  {
    segmentationPathGenerator->increment_index();
    ImagePersister::save_image_on_thread(colouredDepthInput, segmentationPathGenerator->make_path("cdepth%06i.png"));
    ImagePersister::save_image_on_thread(colouredDepthMasked, segmentationPathGenerator->make_path("cdepthm%06i.png"));
    ImagePersister::save_image_on_thread(depthInput, segmentationPathGenerator->make_path("depth%06i.pgm"));
    ImagePersister::save_image_on_thread(depthMasked, segmentationPathGenerator->make_path("depthm%06i.pgm"));
    ImagePersister::save_image_on_thread(rgbInput, segmentationPathGenerator->make_path("rgb%06i.ppm"));
    ImagePersister::save_image_on_thread(rgbMasked, segmentationPathGenerator->make_path("rgbm%06i.ppm"));
  }

  // Set the masked colour image as the segmentation overlay image so that it will be rendered.
  m_context->set_segmentation_image(m_sceneID, rgbMasked);
}

void ObjectSegmentationComponent::run_segmentation_training(const VoxelRenderState_CPtr& renderState)
{
  const Segmenter_Ptr& segmenter = get_segmenter();
  if(!segmenter) return;

  ORUChar4Image_CPtr segmentationImage = segmenter->train(m_context->get_slam_state(m_sceneID)->get_pose(), renderState);
  m_context->set_segmentation_image(m_sceneID, segmentationImage);
}

void ObjectSegmentationComponent::toggle_output()
{
  m_outputEnabled = !m_outputEnabled;
}

//#################### PRIVATE MEMBER FUNCTIONS ####################

const Segmenter_Ptr& ObjectSegmentationComponent::get_segmenter() const
{
#if WITH_ARRAYFIRE && WITH_OPENCV
  if(!m_context->get_segmenter())
  {
    const TouchSettings_Ptr touchSettings(new TouchSettings(m_context->get_resources_dir() + "/TouchSettings.xml"));
    m_context->set_segmenter(Segmenter_Ptr(new BackgroundSubtractingObjectSegmenter(m_context->get_slam_state(m_sceneID)->get_view(), m_context->get_settings(), touchSettings)));
  }
#endif

  return m_context->get_segmenter();
}

}
