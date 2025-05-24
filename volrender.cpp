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
  auto [amrDataSet, fieldName] = getPhysicsCodeProxyData();

  // Set up a camera for rendering the input data
  viskores::rendering::Camera camera;
  camera.SetLookAt(viskores::Vec3f_32(0.5, 0.5, 0.5));
  camera.SetViewUp(viskores::make_Vec(0.f, 1.f, 0.f));
  camera.SetClippingRange(1.f, 10.f);
  camera.SetFieldOfView(60.f);
  camera.SetPosition(viskores::Vec3f_32(1.5, 1.5, 1.5));

  // Set up color table and opacity
  viskores::cont::ColorTable colorTable("inferno");
  colorTable.AddPointAlpha(0., 0.);
  colorTable.AddPointAlpha(0.5, 0.2);
  colorTable.AddPointAlpha(1.0, 0.2);

  // Set up Actor
  viskores::rendering::Actor actor(amrDataSet, fieldName, fieldName, colorTable);
  viskores::rendering::Scene scene;
  scene.AddActor(actor);

  // Render the input data using OSMesa
  viskores::rendering::Color bg(0.2f, 0.2f, 0.2f, 1.0f);
  viskores::rendering::CanvasRayTracer canvas(1024, 1024);

  // render volume
  {
    viskores::rendering::View3D viewVolume(
        scene, viskores::rendering::MapperVolume(), canvas, camera, bg);
    viewVolume.Paint();
    viewVolume.SaveAs("volume.png");
  }

  // render surface
  {
    viskores::rendering::View3D viewSurface(
        scene, viskores::rendering::MapperRayTracer(), canvas, camera, bg);
    viewSurface.Paint();
    viewSurface.SaveAs("surface.png");
  }

  return 0;
}
