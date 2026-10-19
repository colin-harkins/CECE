#include <gtest/gtest.h>

#include "cece/cece_regridder_utils.hpp"

TEST(RegridMethodResolutionTest, ResolvesAliasesToCanonicalMethods) {
    using cece::io::RegridMethod;
    EXPECT_EQ(cece::io::resolve_regrid_method("nearest").method, RegridMethod::Nearest);
    EXPECT_EQ(cece::io::resolve_regrid_method("near").method, RegridMethod::Nearest);
    EXPECT_EQ(cece::io::resolve_regrid_method("NN").method, RegridMethod::Nearest);
    EXPECT_EQ(cece::io::resolve_regrid_method("bilinear").method, RegridMethod::Bilinear);
    EXPECT_EQ(cece::io::resolve_regrid_method("bi").method, RegridMethod::Bilinear);
    EXPECT_EQ(cece::io::resolve_regrid_method("bilin").method, RegridMethod::Bilinear);
    EXPECT_EQ(cece::io::resolve_regrid_method("cubic").method, RegridMethod::Bicubic);
    EXPECT_EQ(cece::io::resolve_regrid_method("bicubic").method, RegridMethod::Bicubic);
    EXPECT_EQ(cece::io::resolve_regrid_method("cu").method, RegridMethod::Bicubic);
    EXPECT_EQ(cece::io::resolve_regrid_method("consd").method, RegridMethod::Conservative1);
    EXPECT_EQ(cece::io::resolve_regrid_method("conservative").method, RegridMethod::Conservative1);
    EXPECT_EQ(cece::io::resolve_regrid_method("cons").method, RegridMethod::Conservative1);
    EXPECT_EQ(cece::io::resolve_regrid_method("conservative1st").method, RegridMethod::Conservative1);
    EXPECT_EQ(cece::io::resolve_regrid_method("cons2nd").method, RegridMethod::Conservative2);
    EXPECT_EQ(cece::io::resolve_regrid_method("conservative2nd").method, RegridMethod::Conservative2);
    EXPECT_EQ(cece::io::resolve_regrid_method("consf").method, RegridMethod::Conservative1);
    EXPECT_EQ(cece::io::resolve_regrid_method("redist").method, RegridMethod::Conservative1);
    EXPECT_EQ(cece::io::resolve_regrid_method("passthrough").method, RegridMethod::Passthrough);
}

TEST(RegridMethodResolutionTest, RejectsUnknownMethodsWithStageContext) {
    try {
        (void)cece::io::resolve_regrid_method("not-a-method");
        FAIL() << "expected unknown method to fail";
    } catch (const cece::io::RegridError& error) {
        EXPECT_EQ(error.stage(), cece::io::RegridStage::PrerequisiteValidation);
        EXPECT_EQ(error.method(), "not-a-method");
        EXPECT_NE(std::string(error.what()).find("Use nearest"), std::string::npos);
    }
}

TEST(RegridPrerequisiteTest, RejectsInvalidLatitudeBeforePlanBuild) {
    cece::io::GridGeometry source, destination;
    for (auto* grid : {&source, &destination}) {
        grid->nx = 1;
        grid->ny = 1;
        grid->center_lon = {0.0};
        grid->center_lat = {0.0};
    }
    source.center_lat[0] = 91.0;
    const auto method = cece::io::resolve_regrid_method("nn");
    EXPECT_THROW(cece::io::validate_regrid_prerequisites(source, destination, method, 0, 1), cece::io::RegridError);
}

TEST(RegridPrerequisiteTest, ConservativeRequiresCompatibleDeclaredGeometry) {
    cece::io::GridGeometry source, destination;
    for (auto* grid : {&source, &destination}) {
        grid->nx = 1;
        grid->ny = 1;
        grid->center_lon = {0.0};
        grid->center_lat = {0.0};
        grid->has_cell_geometry = true;
        grid->metadata.provenance = axis::topology::GeometryProvenance::SourceAuthoritative;
        grid->metadata.boundary_model = axis::topology::BoundaryModel::GreatCircle;
        grid->metadata.area_model = axis::topology::AreaModel::SphericalExcess;
    }
    const auto method = cece::io::resolve_regrid_method("consd");
    EXPECT_NO_THROW(cece::io::validate_regrid_prerequisites(source, destination, method, 0, 1));

    source.metadata.provenance = axis::topology::GeometryProvenance::Reconstructed;
    EXPECT_THROW(cece::io::validate_regrid_prerequisites(source, destination, method, 0, 1), cece::io::RegridError);
}

TEST(RegridPrerequisiteTest, RejectsSecondOrderConstantLatitudeStrips) {
    cece::io::GridGeometry source, destination;
    for (auto* grid : {&source, &destination}) {
        grid->nx = 1;
        grid->ny = 1;
        grid->center_lon = {0.0};
        grid->center_lat = {0.0};
        grid->has_cell_geometry = true;
        grid->metadata.provenance = axis::topology::GeometryProvenance::DeclaredGridModel;
        grid->metadata.boundary_model = axis::topology::BoundaryModel::ConstantLatitude;
        grid->metadata.area_model = axis::topology::AreaModel::ConstantLatitudeStrip;
    }
    const auto method = cece::io::resolve_regrid_method("conss");
    EXPECT_THROW(cece::io::validate_regrid_prerequisites(source, destination, method, 0, 1), cece::io::RegridError);
}

TEST(RegridPrerequisiteTest, BilinearRequiresStructuredSourceStencil) {
    cece::io::GridGeometry source, destination;
    source.nx = 4;
    source.ny = 1;
    source.center_lon = {0.0, 1.0, 2.0, 3.0};
    source.center_lat = {0.0};
    destination.nx = 2;
    destination.ny = 1;
    destination.center_lon = {0.5, 2.5};
    destination.center_lat = {0.0};
    const auto method = cece::io::resolve_regrid_method("bi");
    EXPECT_THROW(cece::io::validate_regrid_prerequisites(source, destination, method, 0, 1), cece::io::RegridError);
}

TEST(RegridPlanKeyTest, IncludesGeometryRevisionMethodAndPartition) {
    cece::io::ValidatedRegridRequest first;
    first.settings = cece::io::resolve_regrid_method("nn");
    first.source_revision = 10;
    first.destination_revision = 20;
    first.j0 = 0;
    first.j1 = 4;
    auto same = first;
    EXPECT_EQ(first.plan_key(), same.plan_key());

    same.destination_revision++;
    EXPECT_NE(first.plan_key(), same.plan_key());
    same = first;
    same.j0++;
    EXPECT_NE(first.plan_key(), same.plan_key());
}

TEST(GridGeometryPreparationTest, KeepsSamplesCenterOnlyByDefaultAndReconstructsOnlyWhenDeclared) {
    auto geometry = std::make_shared<cece::io::GridGeometry>();
    geometry->nx = 2;
    geometry->ny = 2;
    geometry->center_lon = {0.0, 3.0};
    geometry->center_lat = {-4.0, 5.0};
    cece::io::GeometrySettings settings;
    auto sample_only = cece::io::prepare_grid_geometry(geometry, settings);
    EXPECT_FALSE(sample_only.cells.has_value());
    EXPECT_EQ(sample_only.geometry->center_lon, geometry->center_lon);

    settings.reconstruction = cece::io::ReconstructionModel::RectilinearMidpoint;
    auto prepared = cece::io::prepare_grid_geometry(geometry, settings);
    ASSERT_TRUE(prepared.cells.has_value());
    EXPECT_EQ(prepared.cells->n_cells(), 4u);
    EXPECT_EQ(prepared.metadata.provenance, axis::topology::GeometryProvenance::DeclaredGridModel);
    EXPECT_FALSE(geometry->has_cell_geometry);
}

TEST(GridGeometryPreparationTest, RejectsMalformedAdvertisedBounds) {
    auto geometry = std::make_shared<cece::io::GridGeometry>();
    geometry->nx = 2;
    geometry->ny = 2;
    geometry->center_lon = {0.0, 3.0};
    geometry->center_lat = {-4.0, 5.0};
    geometry->has_explicit_bounds = true;
    cece::io::GeometrySettings settings;
    EXPECT_THROW(cece::io::prepare_grid_geometry(geometry, settings), cece::io::RegridError);
}
