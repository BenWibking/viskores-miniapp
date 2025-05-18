#include <viskores/cont/Initialize.h>
#include <viskores/cont/PartitionedDataSet.h>
#include <viskores/cont/MergePartitionedDataSet.h>
#include <viskores/filter/contour/Contour.h>
#include <viskores/filter/field_conversion/PointAverage.h>
#include <viskores/filter/clean_grid/CleanGrid.h>
#include <viskores/rendering/Actor.h>
#include <viskores/rendering/CanvasRayTracer.h>
#include <viskores/rendering/MapperRayTracer.h>
#include <viskores/rendering/MapperWireframer.h>
#include <viskores/rendering/Scene.h>
#include <viskores/rendering/View3D.h>

#include "data.hpp"

int main(int argc, char *argv[]) {
  viskores::cont::Initialize(argc, argv, viskores::cont::InitializeOptions::Strict);
  auto [derivedDataSet, fieldName] = getPhysicsCodeProxyData();
  
  // Compute isosurfaces for each partition
  viskores::filter::field_conversion::PointAverage toPoints;
  toPoints.SetActiveField(fieldName);
  viskores::filter::contour::Contour contourFilter;
  contourFilter.SetIsoValue(150.);
  contourFilter.SetActiveField(fieldName);
  
  std::vector<viskores::cont::DataSet> surfaces;
  for (int i = 0; i < derivedDataSet.GetNumberOfPartitions(); ++i) {
    viskores::cont::DataSet const &thisData = derivedDataSet.GetPartition(i);
    viskores::cont::DataSet const nodalData = toPoints.Execute(thisData);
    viskores::cont::DataSet const isoData = contourFilter.Execute(nodalData);
    surfaces.emplace_back(std::move(isoData));
  }

  // Create a merged data set
  viskores::cont::PartitionedDataSet surfaceDataSet;
  for (auto const &surface: surfaces) {
    surfaceDataSet.AppendPartition(surface);
  }
  viskores::cont::DataSet rawIsoData =
    viskores::cont::MergePartitionedDataSet(surfaceDataSet);

  // Merge overlapping points
  viskores::filter::clean_grid::CleanGrid clean;
  viskores::cont::DataSet isoData = clean.Execute(rawIsoData);
  isoData.PrintSummary(std::cout);
  
  // Set up a camera for rendering the input data
  viskores::rendering::Camera camera;
  camera.SetLookAt(viskores::Vec3f_32(0.5, 0.5, 0.5));
  camera.SetViewUp(viskores::make_Vec(0.f, 1.f, 0.f));
  camera.SetClippingRange(1.f, 10.f);
  camera.SetFieldOfView(60.f);
  camera.SetPosition(viskores::Vec3f_32(1.5, 1.5, 1.5));
  viskores::cont::ColorTable colorTable("inferno");

  // Render an image with the output isosurface
  viskores::rendering::Actor isoActor(
      isoData.GetCellSet(),
      isoData.GetCoordinateSystem(),
      isoData.GetField(fieldName), colorTable);

  viskores::rendering::Scene isoScene;
  isoScene.AddActor(std::move(isoActor));
  
  viskores::rendering::Color bg(0.2f, 0.2f, 0.2f, 1.0f);
  viskores::rendering::CanvasRayTracer canvas(1024, 1024);
  
  // Wireframe surface
  viskores::rendering::View3D isoView(isoScene, viskores::rendering::MapperWireframer(), canvas,
                                      camera, bg);
  isoView.Paint();
  isoView.SaveAs("isosurface_wireframer.png");

  // Shaded surface
  viskores::rendering::View3D solidView(isoScene, viskores::rendering::MapperRayTracer(), canvas,
                                        camera, bg);
  solidView.Paint();
  solidView.SaveAs("isosurface_raytracer.png");

  return 0;
}
