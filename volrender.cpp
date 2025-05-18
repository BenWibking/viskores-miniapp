#include <viskores/cont/Initialize.h>
#include <viskores/cont/MergePartitionedDataSet.h>
#include <viskores/cont/PartitionedDataSet.h>
#include <viskores/filter/entity_extraction/Threshold.h>
#include <viskores/rendering/Actor.h>
#include <viskores/rendering/CanvasRayTracer.h>
#include <viskores/rendering/MapperRayTracer.h>
#include <viskores/rendering/MapperVolume.h>
#include <viskores/rendering/Scene.h>
#include <viskores/rendering/View3D.h>

#include "data.hpp"

int main(int argc, char *argv[]) {
  viskores::cont::Initialize(argc, argv, viskores::cont::InitializeOptions::Strict);
  auto [derivedDataSet, fieldName] = getPhysicsCodeProxyData();

  viskores::cont::DataSet mergedData =
      viskores::cont::MergePartitionedDataSet(derivedDataSet);

  // Set up a camera for rendering the input data
  viskores::rendering::Camera camera;
  camera.SetLookAt(viskores::Vec3f_32(0.5, 0.5, 0.5));
  camera.SetViewUp(viskores::make_Vec(0.f, 1.f, 0.f));
  camera.SetClippingRange(1.f, 10.f);
  camera.SetFieldOfView(60.f);
  camera.SetPosition(viskores::Vec3f_32(1.5, 1.5, 1.5));
  viskores::cont::ColorTable colorTable("inferno");

  // Set up Actor
  viskores::rendering::Actor actor(mergedData.GetCellSet(),
                                   mergedData.GetCoordinateSystem(),
                                   mergedData.GetField(fieldName), colorTable);
  viskores::rendering::Scene scene;
  scene.AddActor(actor);

  // Render the input data using OSMesa
  viskores::rendering::Color bg(0.2f, 0.2f, 0.2f, 1.0f);
  viskores::rendering::CanvasRayTracer canvas(1024, 1024);

  // render surface
  viskores::rendering::View3D viewSurface(
      scene, viskores::rendering::MapperRayTracer(), canvas, camera, bg);
  viewSurface.Paint();
  viewSurface.SaveAs("surface.png");

#if 0
  // render volume (NOT SUPPORTED BY VISKORES)
  viskores::rendering::View3D viewVolume(
      scene, viskores::rendering::MapperVolume(), canvas, camera, bg);
  viewVolume.Paint();
  viewVolume.SaveAs("volume.png");
#endif
  
  return 0;
}
