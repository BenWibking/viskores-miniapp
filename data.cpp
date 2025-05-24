#include <viskores/source/Amr.h>

auto getPhysicsCodeProxyData() -> std::tuple<viskores::cont::PartitionedDataSet, std::string> {
  // Generate AMR dataset
  const int nx = 8;
  const int nlevels = 3;
  viskores::source::Amr amrDataGenerator{};
  amrDataGenerator.SetDimension(3); // 3D
  amrDataGenerator.SetCellsPerDimension(nx);
  amrDataGenerator.SetNumberOfLevels(nlevels);
  viskores::cont::PartitionedDataSet const amrData = amrDataGenerator.Execute();

  std::string fieldName = "RTDataCells";
  return std::make_tuple(amrData, fieldName);
}
