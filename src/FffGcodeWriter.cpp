
#include "FffGcodeWriter.h"
#include "Progress.h"
#include "wallOverlap.h"

namespace cura
{


void FffGcodeWriter::writeGCode(SliceDataStorage& storage, TimeKeeper& time_keeper)
{
    gcode.preSetup(*this);
    
    gcode.resetTotalPrintTimeAndFilament();
    
    if (command_socket)
        command_socket->beginGCode();

    setConfigCoasting();

    setConfigRetraction(storage);

    if (file_number == 1)
    {
        processStartingCode(storage);
    }
    else
    {
        processNextPrintObjectCode(storage);
    }
    file_number++;

    unsigned int total_layers = storage.meshes[0].layers.size();
    //gcode.writeComment("Layer count: %d", totalLayers);

    bool has_raft = getSettingAsPlatformAdhesion("adhesion_type") == Adhesion_Raft;
    if (has_raft)
    {
        processRaft(storage, total_layers);
    }
    
    for(unsigned int layer_nr=0; layer_nr<total_layers; layer_nr++)
    {
        processLayer(storage, layer_nr, total_layers, has_raft);
    }
    
    gcode.writeRetraction(&storage.retraction_config, true);

    Progress::messageProgressStage(Progress::Stage::FINISH, &time_keeper, command_socket);
    
    gcode.writeFanCommand(0);

    //Store the object height for when we are printing multiple objects, as we need to clear every one of them when moving to the next position.
    max_object_height = std::max(max_object_height, storage.model_max.z);
    
    if (command_socket)
    {
        finalize();
        command_socket->sendGCodeLayer();
        command_socket->endSendSlicedObject();
        if (gcode.getFlavor() == GCODE_FLAVOR_ULTIGCODE)
        {
            std::ostringstream prefix;
            prefix << ";FLAVOR:UltiGCode\n";
            prefix << ";TIME:" << int(gcode.getTotalPrintTime()) << "\n";
            prefix << ";MATERIAL:" << int(gcode.getTotalFilamentUsed(0)) << "\n";
            prefix << ";MATERIAL2:" << int(gcode.getTotalFilamentUsed(1)) << "\n";
            command_socket->sendGCodePrefix(prefix.str());
        }
    }
}


void FffGcodeWriter::setConfigCoasting() 
{
    coasting_config.coasting_enable = getSettingBoolean("coasting_enable"); 
    coasting_config.coasting_volume_move = getSettingInCubicMillimeters("coasting_volume_move"); 
    coasting_config.coasting_min_volume_move = getSettingInCubicMillimeters("coasting_min_volume_move"); 
    coasting_config.coasting_speed_move = getSettingInPercentage("coasting_speed_move"); 

    coasting_config.coasting_volume_retract = getSettingInCubicMillimeters("coasting_volume_retract");
    coasting_config.coasting_min_volume_retract = getSettingInCubicMillimeters("coasting_min_volume_retract");
    coasting_config.coasting_speed_retract = getSettingInPercentage("coasting_speed_retract");
}

void FffGcodeWriter::setConfigRetraction(SliceDataStorage& storage) 
{
    storage.retraction_config.amount = INT2MM(getSettingInMicrons("retraction_amount"));
    storage.retraction_config.primeAmount = INT2MM(getSettingInMicrons("retraction_extra_prime_amount"));
    storage.retraction_config.speed = getSettingInMillimetersPerSecond("retraction_retract_speed");
    storage.retraction_config.primeSpeed = getSettingInMillimetersPerSecond("retraction_prime_speed");
    storage.retraction_config.zHop = getSettingInMicrons("retraction_hop");
    for(SliceMeshStorage& mesh : storage.meshes)
    {
        mesh.retraction_config = storage.retraction_config;
    }
}

void FffGcodeWriter::setConfigSkirt(SliceDataStorage& storage, int layer_thickness)
{
    storage.skirt_config.setSpeed(getSettingInMillimetersPerSecond("skirt_speed"));
    storage.skirt_config.setLineWidth(getSettingInMicrons("skirt_line_width"));
    storage.skirt_config.setFlow(getSettingInPercentage("material_flow"));
    storage.skirt_config.setLayerHeight(layer_thickness);
}

void FffGcodeWriter::setConfigSupport(SliceDataStorage& storage, int layer_thickness)
{
    storage.support_config.setLineWidth(getSettingInMicrons("support_line_width"));
    storage.support_config.setSpeed(getSettingInMillimetersPerSecond("speed_support_lines"));
    storage.support_config.setFlow(getSettingInPercentage("material_flow"));
    storage.support_config.setLayerHeight(layer_thickness);
    
    storage.support_roof_config.setLineWidth(getSettingInMicrons("support_roof_line_width"));
    storage.support_roof_config.setSpeed(getSettingInMillimetersPerSecond("speed_support_roof"));
    storage.support_roof_config.setFlow(getSettingInPercentage("material_flow"));
    storage.support_roof_config.setLayerHeight(layer_thickness);
}

void FffGcodeWriter::setConfigInsets(SliceMeshStorage& mesh, int layer_thickness)
{
    mesh.inset0_config.setLineWidth(mesh.settings->getSettingInMicrons("wall_line_width_0"));
    mesh.inset0_config.setSpeed(mesh.settings->getSettingInMillimetersPerSecond("speed_wall_0"));
    mesh.inset0_config.setFlow(mesh.settings->getSettingInPercentage("material_flow"));
    mesh.inset0_config.setLayerHeight(layer_thickness);

    mesh.insetX_config.setLineWidth(mesh.settings->getSettingInMicrons("wall_line_width_x"));
    mesh.insetX_config.setSpeed(mesh.settings->getSettingInMillimetersPerSecond("speed_wall_x"));
    mesh.insetX_config.setFlow(mesh.settings->getSettingInPercentage("material_flow"));
    mesh.insetX_config.setLayerHeight(layer_thickness);
}

void FffGcodeWriter::setConfigSkin(SliceMeshStorage& mesh, int layer_thickness)
{
    mesh.skin_config.setLineWidth(mesh.settings->getSettingInMicrons("skin_line_width"));
    mesh.skin_config.setSpeed(mesh.settings->getSettingInMillimetersPerSecond("speed_topbottom"));
    mesh.skin_config.setFlow(mesh.settings->getSettingInPercentage("material_flow"));
    mesh.skin_config.setLayerHeight(layer_thickness);
}

void FffGcodeWriter::setConfigInfill(SliceMeshStorage& mesh, int layer_thickness)
{
    for(unsigned int idx=0; idx<MAX_SPARSE_COMBINE; idx++)
    {
        mesh.infill_config[idx].setLineWidth(mesh.settings->getSettingInMicrons("infill_line_width") * (idx + 1));
        mesh.infill_config[idx].setSpeed(mesh.settings->getSettingInMillimetersPerSecond("speed_infill"));
        mesh.infill_config[idx].setFlow(mesh.settings->getSettingInPercentage("material_flow"));
        mesh.infill_config[idx].setLayerHeight(layer_thickness);
    }
}

void FffGcodeWriter::processStartingCode(SliceDataStorage& storage)
{
    if (gcode.getFlavor() == GCODE_FLAVOR_ULTIGCODE)
    {
        if (!command_socket)
        {
            gcode.writeCode(";FLAVOR:UltiGCode\n;TIME:666\n;MATERIAL:666\n;MATERIAL2:-1\n");
        }
    }
    else 
    {
        if (getSettingInDegreeCelsius("material_bed_temperature") > 0)
            gcode.writeBedTemperatureCommand(getSettingInDegreeCelsius("material_bed_temperature"), true);
        
        for(SliceMeshStorage& mesh : storage.meshes)
            if (mesh.settings->getSettingInDegreeCelsius("material_print_temperature") > 0)
                gcode.writeTemperatureCommand(mesh.settings->getSettingAsIndex("extruder_nr"), mesh.settings->getSettingInDegreeCelsius("material_print_temperature"));
        for(SliceMeshStorage& mesh : storage.meshes)
            if (mesh.settings->getSettingInDegreeCelsius("material_print_temperature") > 0)
                gcode.writeTemperatureCommand(mesh.settings->getSettingAsIndex("extruder_nr"), mesh.settings->getSettingInDegreeCelsius("material_print_temperature"), true);
    }
    
    gcode.writeCode(getSettingString("machine_start_gcode").c_str());

    gcode.writeComment("Generated with Cura_SteamEngine " VERSION);
    if (gcode.getFlavor() == GCODE_FLAVOR_BFB)
    {
        gcode.writeComment("enable auto-retraction");
        std::ostringstream tmp;
        tmp << "M227 S" << (getSettingInMicrons("retraction_amount") * 2560 / 1000) << " P" << (getSettingInMicrons("retraction_amount") * 2560 / 1000);
        gcode.writeLine(tmp.str().c_str());
    }
}

void FffGcodeWriter::processNextPrintObjectCode(SliceDataStorage& storage)
{
    gcode.writeFanCommand(0);
    gcode.resetExtrusionValue();
    gcode.setZ(max_object_height + 5000);
    gcode.writeMove(gcode.getPositionXY(), getSettingInMillimetersPerSecond("speed_travel"), 0);
    gcode.writeMove(Point(storage.model_min.x, storage.model_min.y), getSettingInMillimetersPerSecond("speed_travel"), 0);
}
    
void FffGcodeWriter::processRaft(SliceDataStorage& storage, unsigned int totalLayers)
{
    GCodePathConfig raft_base_config(&storage.retraction_config, "SUPPORT");
    raft_base_config.setSpeed(getSettingInMillimetersPerSecond("raft_base_speed"));
    raft_base_config.setLineWidth(getSettingInMicrons("raft_base_line_width"));
    raft_base_config.setLayerHeight(getSettingInMicrons("raft_base_thickness"));
    raft_base_config.setFlow(getSettingInPercentage("material_flow"));
    GCodePathConfig raft_interface_config(&storage.retraction_config, "SUPPORT");
    raft_interface_config.setSpeed(getSettingInMillimetersPerSecond("raft_interface_speed"));
    raft_interface_config.setLineWidth(getSettingInMicrons("raft_interface_line_width"));
    raft_interface_config.setLayerHeight(getSettingInMicrons("raft_base_thickness"));
    raft_interface_config.setFlow(getSettingInPercentage("material_flow"));
    GCodePathConfig raft_surface_config(&storage.retraction_config, "SUPPORT");
    raft_surface_config.setSpeed(getSettingInMillimetersPerSecond("raft_surface_speed"));
    raft_surface_config.setLineWidth(getSettingInMicrons("raft_surface_line_width"));
    raft_surface_config.setLayerHeight(getSettingInMicrons("raft_base_thickness"));
    raft_surface_config.setFlow(getSettingInPercentage("material_flow"));

    { // raft base layer
        gcode.writeLayerComment(-3);
        gcode.writeComment("RAFT");
        GCodePlanner gcodeLayer(gcode, storage, &storage.retraction_config, coasting_config, getSettingInMillimetersPerSecond("speed_travel"), getSettingInMicrons("retraction_min_travel"), getSettingBoolean("retraction_combing"), 0, getSettingInMicrons("wall_line_width_0"), getSettingBoolean("travel_avoid_other_parts"), getSettingInMicrons("travel_avoid_distance"));
        if (getSettingAsIndex("support_extruder_nr") > 0)
            gcodeLayer.setExtruder(getSettingAsIndex("support_extruder_nr"));
        gcode.setZ(getSettingInMicrons("raft_base_thickness"));
        gcodeLayer.addPolygonsByOptimizer(storage.raftOutline, &raft_base_config);

        Polygons raftLines;
        int offset_from_poly_outline = 0;
        generateLineInfill(storage.raftOutline, offset_from_poly_outline, raftLines, getSettingInMicrons("raft_base_line_width"), getSettingInMicrons("raft_base_line_spacing"), getSettingInPercentage("fill_overlap"), 0);
        gcodeLayer.addLinesByOptimizer(raftLines, &raft_base_config);

        gcode.writeFanCommand(getSettingInPercentage("raft_base_fan_speed"));
        gcodeLayer.writeGCode(false, getSettingInMicrons("raft_base_thickness"));
    }

    { // raft interface layer
        gcode.writeLayerComment(-2);
        gcode.writeComment("RAFT");
        GCodePlanner gcodeLayer(gcode, storage, &storage.retraction_config, coasting_config, getSettingInMillimetersPerSecond("speed_travel"), getSettingInMicrons("retraction_min_travel"), getSettingBoolean("retraction_combing"), 0, getSettingInMicrons("wall_line_width_0"), getSettingBoolean("travel_avoid_other_parts"), getSettingInMicrons("travel_avoid_distance"));
        gcode.setZ(getSettingInMicrons("raft_base_thickness") + getSettingInMicrons("raft_interface_thickness"));

        Polygons raftLines;
        int offset_from_poly_outline = 0;
        generateLineInfill(storage.raftOutline, offset_from_poly_outline, raftLines, getSettingInMicrons("raft_interface_line_width"), getSettingInMicrons("raft_interface_line_spacing"), getSettingInPercentage("fill_overlap"), getSettingAsCount("raft_surface_layers") > 0 ? 45 : 90);
        gcodeLayer.addLinesByOptimizer(raftLines, &raft_interface_config);

        gcodeLayer.writeGCode(false, getSettingInMicrons("raft_interface_thickness"));
    }

    for (int raftSurfaceLayer=1; raftSurfaceLayer<=getSettingAsCount("raft_surface_layers"); raftSurfaceLayer++)
    { // raft surface layers
        gcode.writeLayerComment(-1);
        gcode.writeComment("RAFT");
        GCodePlanner gcodeLayer(gcode, storage, &storage.retraction_config, coasting_config, getSettingInMillimetersPerSecond("speed_travel"), getSettingInMicrons("retraction_min_travel"), getSettingBoolean("retraction_combing"), 0, getSettingInMicrons("wall_line_width_0"), getSettingBoolean("travel_avoid_other_parts"), getSettingInMicrons("travel_avoid_distance"));
        gcode.setZ(getSettingInMicrons("raft_base_thickness") + getSettingInMicrons("raft_interface_thickness") + getSettingInMicrons("raft_surface_thickness")*raftSurfaceLayer);

        Polygons raft_lines;
        int offset_from_poly_outline = 0;
        generateLineInfill(storage.raftOutline, offset_from_poly_outline, raft_lines, getSettingInMicrons("raft_surface_line_width"), getSettingInMicrons("raft_surface_line_spacing"), getSettingInPercentage("fill_overlap"), 90 * raftSurfaceLayer);
        gcodeLayer.addLinesByOptimizer(raft_lines, &raft_surface_config);

        gcodeLayer.writeGCode(false, getSettingInMicrons("raft_interface_thickness"));
    }
}

void FffGcodeWriter::processLayer(SliceDataStorage& storage, unsigned int layer_nr, unsigned int total_layers, bool has_raft)
{
    Progress::messageProgress(Progress::Stage::EXPORT, layer_nr+1, total_layers, command_socket);

    int layer_thickness = getSettingInMicrons("layer_height");
    if (layer_nr == 0 && !has_raft)
    {
        layer_thickness = getSettingInMicrons("layer_height_0");
    }
    
    

    setConfigSkirt(storage, layer_thickness);

    setConfigSupport(storage, layer_thickness);
    
    for(SliceMeshStorage& mesh : storage.meshes)
    {
        setConfigInsets(mesh, layer_thickness);
        setConfigSkin(mesh, layer_thickness);
        setConfigInfill(mesh, layer_thickness);
    }

    processInitialLayersSpeedup(storage, layer_nr);

    gcode.writeLayerComment(layer_nr);

    GCodePlanner gcode_layer(gcode, storage, &storage.retraction_config, coasting_config, getSettingInMillimetersPerSecond("speed_travel"), getSettingInMicrons("retraction_min_travel"), getSettingBoolean("retraction_combing"), layer_nr, getSettingInMicrons("wall_line_width_0"), getSettingBoolean("travel_avoid_other_parts"), getSettingInMicrons("travel_avoid_distance"));
    
    if (!getSettingBoolean("retraction_combing")) 
        gcode_layer.setAlwaysRetract(true);

    int z = storage.meshes[0].layers[layer_nr].printZ;         
    gcode.setZ(z);
    gcode.resetStartPosition();
    
    processSkirt(storage, gcode_layer, layer_nr);
    
    int support_extruder_nr = (layer_nr == 0)? getSettingAsIndex("support_extruder_nr_layer_1") : getSettingAsIndex("support_extruder_nr");
    bool printSupportFirst = (storage.support.generated && support_extruder_nr > 0 && support_extruder_nr == gcode_layer.getExtruder());
    if (printSupportFirst)
        addSupportToGCode(storage, gcode_layer, layer_nr);

    processOozeShield(storage, gcode_layer, layer_nr);
    
    processDraftShield(storage, gcode_layer, layer_nr);

    //Figure out in which order to print the meshes, do this by looking at the current extruder and preferer the meshes that use that extruder.
    std::vector<SliceMeshStorage*> mesh_order = calculateMeshOrder(storage, gcode_layer.getExtruder());
    for(SliceMeshStorage* mesh : mesh_order)
    {
        if (getSettingBoolean("magic_mesh_surface_mode"))
        {
            addMeshLayerToGCode_magicPolygonMode(storage, mesh, gcode_layer, layer_nr);
        }
        else
        {
            addMeshLayerToGCode(storage, mesh, gcode_layer, layer_nr);
        }
    }
    if (!printSupportFirst)
        addSupportToGCode(storage, gcode_layer, layer_nr);

    processFanSpeedAndMinimalLayerTime(storage, gcode_layer, layer_nr);
    
    gcode_layer.writeGCode(getSettingBoolean("cool_lift_head"), layer_nr > 0 ? getSettingInMicrons("layer_height") : getSettingInMicrons("layer_height_0"));
    if (command_socket)
        command_socket->sendGCodeLayer();
}

void FffGcodeWriter::processInitialLayersSpeedup(SliceDataStorage& storage, unsigned int layer_nr)
{
    double initial_speedup_layers = getSettingAsCount("speed_slowdown_layers");
    if (static_cast<int>(layer_nr) < initial_speedup_layers)
    {
        double initial_layer_speed = getSettingInMillimetersPerSecond("speed_layer_0");
        storage.support_config.smoothSpeed(initial_layer_speed, layer_nr, initial_speedup_layers);
        for(SliceMeshStorage& mesh : storage.meshes)
        {
            mesh.inset0_config.smoothSpeed(initial_layer_speed, layer_nr, initial_speedup_layers);
            mesh.insetX_config.smoothSpeed(initial_layer_speed, layer_nr, initial_speedup_layers);
            mesh.skin_config.smoothSpeed(initial_layer_speed, layer_nr, initial_speedup_layers);
            for(unsigned int idx=0; idx<MAX_SPARSE_COMBINE; idx++)
            {
                mesh.infill_config[idx].smoothSpeed(initial_layer_speed, layer_nr, initial_speedup_layers);
            }
        }
    }
}

void FffGcodeWriter::processSkirt(SliceDataStorage& storage, GCodePlanner& gcode_layer, unsigned int layer_nr)
{
    if (layer_nr == 0)
    {
        if (storage.skirt.size() > 0)
            gcode_layer.addTravel(storage.skirt[storage.skirt.size()-1].closestPointTo(gcode.getPositionXY()));
        gcode_layer.addPolygonsByOptimizer(storage.skirt, &storage.skirt_config);
    }
}

void FffGcodeWriter::processOozeShield(SliceDataStorage& storage, GCodePlanner& gcode_layer, unsigned int layer_nr)
{
    if (storage.oozeShield.size() > 0)
    {
        gcode_layer.setAlwaysRetract(true);
        gcode_layer.addPolygonsByOptimizer(storage.oozeShield[layer_nr], &storage.skirt_config);
        gcode_layer.setAlwaysRetract(!getSettingBoolean("retraction_combing"));
    }
}

void FffGcodeWriter::processDraftShield(SliceDataStorage& storage, GCodePlanner& gcode_layer, unsigned int layer_nr)
{
    if (storage.draft_protection_shield.size() == 0)
    {
        return;
    }
    
    int draft_shield_height = getSettingInMicrons("draft_shield_height");
    int layer_height_0 = getSettingInMicrons("layer_height_0");
    int layer_height = getSettingInMicrons("layer_height");
    
    int max_screen_layer = (draft_shield_height - layer_height_0) / layer_height + 1;
    
    if (int(layer_nr) > max_screen_layer)
    {
        return;
    }
    
    gcode_layer.setAlwaysRetract(true);
    gcode_layer.addPolygonsByOptimizer(storage.draft_protection_shield, &storage.skirt_config);
    gcode_layer.setAlwaysRetract(!getSettingBoolean("retraction_combing"));
    
}

std::vector<SliceMeshStorage*> FffGcodeWriter::calculateMeshOrder(SliceDataStorage& storage, int current_extruder)
{
    std::vector<SliceMeshStorage*> ret;
    std::vector<SliceMeshStorage*> add_list;
    for(SliceMeshStorage& mesh : storage.meshes)
        add_list.push_back(&mesh);

    int add_extruder_nr = current_extruder;
    while(add_list.size() > 0)
    {
        for(unsigned int idx=0; idx<add_list.size(); idx++)
        {
            if (add_list[idx]->settings->getSettingAsIndex("extruder_nr") == add_extruder_nr)
            {
                ret.push_back(add_list[idx]);
                add_list.erase(add_list.begin() + idx);
                idx--;
            }
        }
        if (add_list.size() > 0)
            add_extruder_nr = add_list[0]->settings->getSettingAsIndex("extruder_nr");
    }
    return ret;
}


void FffGcodeWriter::addMeshLayerToGCode_magicPolygonMode(SliceDataStorage& storage, SliceMeshStorage* mesh, GCodePlanner& gcode_layer, int layer_nr)
{
    int prevExtruder = gcode_layer.getExtruder();
    bool extruder_changed = gcode_layer.setExtruder(mesh->settings->getSettingAsIndex("extruder_nr"));

    if (layer_nr > mesh->layer_nr_max_filled_layer)
    {
        return;
    }
    
    if (extruder_changed)
        addWipeTower(storage, gcode_layer, layer_nr, prevExtruder);

    SliceLayer* layer = &mesh->layers[layer_nr];


    Polygons polygons;
    for(unsigned int partNr=0; partNr<layer->parts.size(); partNr++)
    {
        for(unsigned int n=0; n<layer->parts[partNr].outline.size(); n++)
        {
            for(unsigned int m=1; m<layer->parts[partNr].outline[n].size(); m++)
            {
                Polygon p;
                p.add(layer->parts[partNr].outline[n][m-1]);
                p.add(layer->parts[partNr].outline[n][m]);
                polygons.add(p);
            }
            if (layer->parts[partNr].outline[n].size() > 0)
            {
                Polygon p;
                p.add(layer->parts[partNr].outline[n][layer->parts[partNr].outline[n].size()-1]);
                p.add(layer->parts[partNr].outline[n][0]);
                polygons.add(p);
            }
        }
    }
    for(unsigned int n=0; n<layer->openLines.size(); n++)
    {
        for(unsigned int m=1; m<layer->openLines[n].size(); m++)
        {
            Polygon p;
            p.add(layer->openLines[n][m-1]);
            p.add(layer->openLines[n][m]);
            polygons.add(p);
        }
    }
    if (mesh->settings->getSettingBoolean("magic_spiralize"))
        mesh->inset0_config.spiralize = true;

    gcode_layer.addPolygonsByOptimizer(polygons, &mesh->inset0_config);
    
}

void FffGcodeWriter::addMeshLayerToGCode(SliceDataStorage& storage, SliceMeshStorage* mesh, GCodePlanner& gcode_layer, int layer_nr)
{
    int previous_extruder = gcode_layer.getExtruder();
    bool extruder_changed = gcode_layer.setExtruder(mesh->settings->getSettingAsIndex("extruder_nr"));

    if (layer_nr > mesh->layer_nr_max_filled_layer)
    {
        return;
    }
    
    if (extruder_changed)
        addWipeTower(storage, gcode_layer, layer_nr, previous_extruder);

    SliceLayer* layer = &mesh->layers[layer_nr];

    PathOrderOptimizer part_order_optimizer(gcode.getStartPositionXY());
    for(unsigned int partNr=0; partNr<layer->parts.size(); partNr++)
    {
        part_order_optimizer.addPolygon(layer->parts[partNr].insets[0][0]);
    }
    part_order_optimizer.optimize();

    bool skin_alternate_rotation = getSettingBoolean("skin_alternate_rotation") && ( getSettingAsCount("top_layers") >= 4 || getSettingAsCount("bottom_layers") >= 4 );
    
    for(int order_idx : part_order_optimizer.polyOrder)
    {
        SliceLayerPart& part = layer->parts[order_idx];

        int fill_angle = 45;
        if (layer_nr & 1)
            fill_angle += 90;
        int extrusion_width =  mesh->infill_config[0].getLineWidth(); //getSettingInMicrons("infill_line_width");
        
        int sparse_infill_line_distance = mesh->settings->getSettingInMicrons("infill_line_distance");
        double infill_overlap = mesh->settings->getSettingInPercentage("fill_overlap");
        
        processMultiLayerInfill(gcode_layer, mesh, part, sparse_infill_line_distance, infill_overlap, fill_angle, extrusion_width);
        processSingleLayerInfill(gcode_layer, mesh, part, sparse_infill_line_distance, infill_overlap, fill_angle, extrusion_width);

        processInsets(gcode_layer, mesh, part, layer_nr);

        if (skin_alternate_rotation && ( layer_nr / 2 ) & 1)
            fill_angle -= 45;
        processSkin(gcode_layer, mesh, part, layer_nr, infill_overlap, fill_angle, extrusion_width);    
    }
}


void FffGcodeWriter::processMultiLayerInfill(GCodePlanner& gcode_layer, SliceMeshStorage* mesh, SliceLayerPart& part, int sparse_infill_line_distance, double infill_overlap, int fill_angle, int extrusion_width)
{
    if (sparse_infill_line_distance > 0)
    {
        //Print the thicker sparse lines first. (double or more layer thickness, infill combined with previous layers)
        for(unsigned int n=1; n<part.sparse_outline.size(); n++)
        {
            Polygons fill_polygons;
            switch(getSettingAsFillMethod("fill_pattern"))
            {
            case Fill_Grid:
                generateGridInfill(part.sparse_outline[n], 0, fill_polygons, extrusion_width, sparse_infill_line_distance * 2, infill_overlap, fill_angle);
                gcode_layer.addLinesByOptimizer(fill_polygons, &mesh->infill_config[n]);
                break;
            case Fill_Lines:
                generateLineInfill(part.sparse_outline[n], 0, fill_polygons, extrusion_width, sparse_infill_line_distance, infill_overlap, fill_angle);
                gcode_layer.addLinesByOptimizer(fill_polygons, &mesh->infill_config[n]);
                break;
            case Fill_Triangles:
                generateTriangleInfill(part.sparse_outline[n], 0, fill_polygons, extrusion_width, sparse_infill_line_distance * 3, infill_overlap, 0);
                gcode_layer.addLinesByOptimizer(fill_polygons, &mesh->infill_config[n]);
                break;
            case Fill_Concentric:
                generateConcentricInfill(part.sparse_outline[n], fill_polygons, sparse_infill_line_distance);
                gcode_layer.addPolygonsByOptimizer(fill_polygons, &mesh->infill_config[n]);
                break;
            case Fill_ZigZag:
                generateZigZagInfill(part.sparse_outline[n], fill_polygons, extrusion_width, sparse_infill_line_distance, infill_overlap, fill_angle, false, false);
                gcode_layer.addPolygonsByOptimizer(fill_polygons, &mesh->infill_config[n]);
                break;
            default:
                logError("fill_pattern has unknown value.\n");
                break;
            }
        }
    }
}

void FffGcodeWriter::processSingleLayerInfill(GCodePlanner& gcode_layer, SliceMeshStorage* mesh, SliceLayerPart& part, int sparse_infill_line_distance, double infill_overlap, int fill_angle, int extrusion_width)
{
    //Combine the 1 layer thick infill with the top/bottom skin and print that as one thing.
    Polygons infill_polygons;
    Polygons infill_lines;
    EFillMethod pattern = getSettingAsFillMethod("fill_pattern");
    if (sparse_infill_line_distance > 0 && part.sparse_outline.size() > 0)
    {
        switch(pattern)
        {
        case Fill_Grid:
            generateGridInfill(part.sparse_outline[0], 0, infill_lines, extrusion_width, sparse_infill_line_distance * 2, infill_overlap, fill_angle);
            break;
        case Fill_Lines:
            generateLineInfill(part.sparse_outline[0], 0, infill_lines, extrusion_width, sparse_infill_line_distance, infill_overlap, fill_angle);
            break;
        case Fill_Triangles:
            generateTriangleInfill(part.sparse_outline[0], 0, infill_lines, extrusion_width, sparse_infill_line_distance * 3, infill_overlap, 0);
            break;
        case Fill_Concentric:
            generateConcentricInfill(part.sparse_outline[0], infill_polygons, sparse_infill_line_distance);
            break;
        case Fill_ZigZag:
            generateZigZagInfill(part.sparse_outline[0], infill_lines, extrusion_width, sparse_infill_line_distance, infill_overlap, fill_angle, false, false);
            break;
        default:
            logError("fill_pattern has unknown value.\n");
            break;
        }
    }
    gcode_layer.addPolygonsByOptimizer(infill_polygons, &mesh->infill_config[0]);
    if (pattern == Fill_Grid || pattern == Fill_Lines || pattern == Fill_Triangles)
    {
        gcode_layer.addLinesByOptimizer(infill_lines, &mesh->infill_config[0], getSettingInMicrons("infill_wipe_dist")); 
    }
    else 
    {
        gcode_layer.addLinesByOptimizer(infill_lines, &mesh->infill_config[0]); 
    }
}

void FffGcodeWriter::processInsets(GCodePlanner& gcode_layer, SliceMeshStorage* mesh, SliceLayerPart& part, unsigned int layer_nr)
{
    bool compensate_overlap = getSettingBoolean("travel_compensate_overlapping_walls_enabled");
    
    if (getSettingAsCount("wall_line_count") > 0)
    {
        if (getSettingBoolean("magic_spiralize"))
        {
            if (static_cast<int>(layer_nr) >= getSettingAsCount("bottom_layers"))
                mesh->inset0_config.spiralize = true;
            if (static_cast<int>(layer_nr) == getSettingAsCount("bottom_layers") && part.insets.size() > 0)
                gcode_layer.addPolygonsByOptimizer(part.insets[0], &mesh->insetX_config);
        }
        for(int inset_number=part.insets.size()-1; inset_number>-1; inset_number--)
        {
            if (inset_number == 0)
            {
                if (!compensate_overlap)
                {
                    gcode_layer.addPolygonsByOptimizer(part.insets[0], &mesh->inset0_config);
                }
                else
                {
                    Polygons& outer_wall = part.insets[0];
                    WallOverlapComputation wall_overlap_computation(outer_wall, getSettingInMicrons("wall_line_width_0"));
                    gcode_layer.addPolygonsByOptimizer(outer_wall, &mesh->inset0_config, &wall_overlap_computation);
                }
            }
            else
            {
                gcode_layer.addPolygonsByOptimizer(part.insets[inset_number], &mesh->insetX_config);
            }
        }
    }
}


void FffGcodeWriter::processSkin(GCodePlanner& gcode_layer, SliceMeshStorage* mesh, SliceLayerPart& part, unsigned int layer_nr, double infill_overlap, int fill_angle, int extrusion_width)
{
    Polygons skin_polygons;
    Polygons skin_lines;
    for(SkinPart& skin_part : part.skin_parts) // TODO: optimize parts order
    {
        int bridge = -1;
        if (layer_nr > 0)
            bridge = bridgeAngle(skin_part.outline, &mesh->layers[layer_nr-1]);
        if (bridge > -1)
        {
            generateLineInfill(skin_part.outline, 0, skin_lines, extrusion_width, extrusion_width, infill_overlap, bridge);
        }else{
            switch(getSettingAsFillMethod("top_bottom_pattern"))
            {
            case Fill_Lines:
                for (Polygons& skin_perimeter : skin_part.insets)
                {
                    gcode_layer.addPolygonsByOptimizer(skin_perimeter, &mesh->skin_config); // add polygons to gcode in inward order
                }
                if (skin_part.insets.size() > 0)
                {
                    generateLineInfill(skin_part.insets.back(), -extrusion_width/2, skin_lines, extrusion_width, extrusion_width, infill_overlap, fill_angle);
                    if (getSettingString("fill_perimeter_gaps") != "Nowhere")
                    {
                        generateLineInfill(skin_part.perimeterGaps, 0, skin_lines, extrusion_width, extrusion_width, 0, fill_angle);
                    }
                } 
                else
                {
                    generateLineInfill(skin_part.outline, 0, skin_lines, extrusion_width, extrusion_width, infill_overlap, fill_angle);
                }
                break;
            case Fill_Concentric:
                {
                    Polygons in_outline;
                    offsetSafe(skin_part.outline, -extrusion_width/2, extrusion_width, in_outline, getSettingBoolean("remove_overlapping_walls_x_enabled"));
                    if (getSettingString("fill_perimeter_gaps") != "Nowhere")
                    {
                        generateConcentricInfillDense(in_outline, skin_polygons, &part.perimeterGaps, extrusion_width, getSettingBoolean("remove_overlapping_walls_x_enabled"));
                    }
                }
                break;
            default:
                logError("Unknown fill method for skin\n");
                break;
            }
        }
    }
    
    // handle gaps between perimeters etc.
    if (getSettingString("fill_perimeter_gaps") != "Nowhere")
    {
        generateLineInfill(part.perimeterGaps, 0, skin_lines, extrusion_width, extrusion_width, 0, fill_angle);
    }
    
    
    gcode_layer.addPolygonsByOptimizer(skin_polygons, &mesh->skin_config);
    gcode_layer.addLinesByOptimizer(skin_lines, &mesh->skin_config);
}

void FffGcodeWriter::addSupportToGCode(SliceDataStorage& storage, GCodePlanner& gcode_layer, int layer_nr)
{
    if (!storage.support.generated)
        return;
    
    
    if (getSettingBoolean("support_roof_enable"))
    {
        int support_extruder_nr = (layer_nr == 0)? getSettingAsIndex("support_extruder_nr_layer_1") : getSettingAsIndex("support_extruder_nr");
        bool print_alternate_material_first = (storage.support.generated && support_extruder_nr > 0 && support_extruder_nr == gcode_layer.getExtruder());
        bool roofs_in_alternate_material_only = true;
        
        if (roofs_in_alternate_material_only && print_alternate_material_first)
        {
            addSupportRoofsToGCode(storage, gcode_layer, layer_nr);
            addSupportLinesToGCode(storage, gcode_layer, layer_nr);
        }
        else 
        {
            addSupportLinesToGCode(storage, gcode_layer, layer_nr);
            addSupportRoofsToGCode(storage, gcode_layer, layer_nr);
        }
    }
    else
    {
        addSupportLinesToGCode(storage, gcode_layer, layer_nr);
    }
}

void FffGcodeWriter::addSupportLinesToGCode(SliceDataStorage& storage, GCodePlanner& gcode_layer, int layer_nr)
{
    if (layer_nr > storage.support.layer_nr_max_filled_layer)
    {
        return;
    }
    
    int support_line_distance = getSettingInMicrons("support_line_distance");
    int extrusion_width = storage.support_config.getLineWidth();
    double infill_overlap = getSettingInPercentage("fill_overlap");
    EFillMethod support_pattern = getSettingAsFillMethod("support_pattern");
    
    if (getSettingAsIndex("support_extruder_nr") > -1)
    {
        int previous_extruder = gcode_layer.getExtruder();
        int support_extruder_nr = (layer_nr == 0)? getSettingAsIndex("support_extruder_nr_layer_1") : getSettingAsIndex("support_extruder_nr");
        bool extruder_changed = gcode_layer.setExtruder(support_extruder_nr);
        
        if (extruder_changed)
            addWipeTower(storage, gcode_layer, layer_nr, previous_extruder);
    }
    Polygons support; // may stay empty
    if (storage.support.generated) 
        support = storage.support.supportLayers[layer_nr].supportAreas;
    
    std::vector<PolygonsPart> support_islands = support.splitIntoParts();

    PathOrderOptimizer island_order_optimizer(gcode.getPositionXY());
    for(unsigned int n=0; n<support_islands.size(); n++)
    {
        island_order_optimizer.addPolygon(support_islands[n][0]);
    }
    island_order_optimizer.optimize();

    for(unsigned int n=0; n<support_islands.size(); n++)
    {
        PolygonsPart& island = support_islands[island_order_optimizer.polyOrder[n]];

        Polygons support_lines;
        if (support_line_distance > 0)
        {
            switch(support_pattern)
            {
            case Fill_Grid:
                {
                    int offset_from_outline = 0;
                    if (support_line_distance > extrusion_width * 4)
                    {
                        generateGridInfill(island, offset_from_outline, support_lines, extrusion_width, support_line_distance*2, infill_overlap, 0);
                    }else{
                        generateLineInfill(island, offset_from_outline, support_lines, extrusion_width, support_line_distance, infill_overlap, (layer_nr & 1) ? 0 : 90);
                    }
                }
                break;
            case Fill_Lines:
                {
                    int offset_from_outline = 0;
                    if (layer_nr == 0)
                    {
                        generateGridInfill(island, offset_from_outline, support_lines, extrusion_width, support_line_distance, 0 + 150, 0);
                    }else{
                        generateLineInfill(island, offset_from_outline, support_lines, extrusion_width, support_line_distance, 0, 0);
                    }
                }
                break;
            case Fill_ZigZag:
                {
                    int offset_from_outline = 0;
                    if (layer_nr == 0)
                    {
                        generateGridInfill(island, offset_from_outline, support_lines, extrusion_width, support_line_distance, 0 + 150, 0);
                    }else{
                        generateZigZagInfill(island, support_lines, extrusion_width, support_line_distance, 0, 0, getSettingBoolean("support_connect_zigzags"), true);
                    }
                }
                break;
            default:
                logError("Unknown fill method for support\n");
                break;
            }
        }

        if (support_pattern == Fill_Grid || ( support_pattern == Fill_ZigZag && layer_nr == 0 ) )
            gcode_layer.addPolygonsByOptimizer(island, &storage.support_config);
        gcode_layer.addLinesByOptimizer(support_lines, &storage.support_config);
    }
}

void FffGcodeWriter::addSupportRoofsToGCode(SliceDataStorage& storage, GCodePlanner& gcodeLayer, int layer_nr)
{
    double fillAngle;
    if (getSettingInMicrons("support_roof_height") < 2 * getSettingInMicrons("layer_height"))
    {
        fillAngle = 90; // perpendicular to support lines
    }
    else 
    {
        fillAngle = 45 + (layer_nr % 2) * 90; // alternate between the two kinds of diagonal:  / and \ .
    }
    double infill_overlap = 0;
    int outline_offset =  0; // - roofConfig.getLineWidth() / 2;
    
    Polygons skinLines;
    generateLineInfill(storage.support.supportLayers[layer_nr].roofs, outline_offset, skinLines, storage.support_roof_config.getLineWidth(), storage.support_roof_config.getLineWidth(), infill_overlap, fillAngle);
    gcodeLayer.addLinesByOptimizer(skinLines, &storage.support_roof_config);
}


void FffGcodeWriter::addWipeTower(SliceDataStorage& storage, GCodePlanner& gcodeLayer, int layer_nr, int prev_extruder)
{
    if (getSettingInMicrons("wipe_tower_size") < 1)
    {
        return;
    }
    if (layer_nr > storage.max_object_height_second_to_last_extruder + 1)
    {
        return;
    }

    int64_t offset = -getSettingInMicrons("wall_line_width_x");
    if (layer_nr > 0)
        offset *= 2;
    
    //If we changed extruder, print the wipe/prime tower for this nozzle;
    std::vector<Polygons> insets;
    if ((layer_nr % 2) == 1)
        insets.push_back(storage.wipeTower.offset(offset / 2));
    else
        insets.push_back(storage.wipeTower);
    while(true)
    {
        Polygons new_inset = insets[insets.size() - 1].offset(offset);
        if (new_inset.size() < 1)
            break;
        insets.push_back(new_inset);
    }
    
    bool wipe_tower_dir_outward = getSettingBoolean("wipe_tower_dir_outward");
    
    for(unsigned int n=0; n<insets.size(); n++)
    {
        gcodeLayer.addPolygonsByOptimizer(insets[(wipe_tower_dir_outward)? insets.size() - 1 - n : n], &storage.meshes[0].insetX_config);
    }
    
    //Make sure we wipe the old extruder on the wipe tower.
    gcodeLayer.addTravel(storage.wipePoint - gcode.getExtruderOffset(prev_extruder) + gcode.getExtruderOffset(gcodeLayer.getExtruder()));
}

void FffGcodeWriter::processFanSpeedAndMinimalLayerTime(SliceDataStorage& storage, GCodePlanner& gcodeLayer, unsigned int layer_nr)
{ 
    double travelTime;
    double extrudeTime;
    gcodeLayer.getTimes(travelTime, extrudeTime);
    gcodeLayer.forceMinimalLayerTime(getSettingInSeconds("cool_min_layer_time"), getSettingInMillimetersPerSecond("cool_min_speed"), travelTime, extrudeTime);

    // interpolate fan speed (for cool_fan_full_layer and for cool_min_layer_time_fan_speed_max)
    double fanSpeed = getSettingInPercentage("cool_fan_speed_min");
    double totalLayerTime = travelTime + extrudeTime;
    if (totalLayerTime < getSettingInSeconds("cool_min_layer_time"))
    {
        fanSpeed = getSettingInPercentage("cool_fan_speed_max");
    }
    else if (totalLayerTime < getSettingInSeconds("cool_min_layer_time_fan_speed_max"))
    { 
        // when forceMinimalLayerTime didn't change the extrusionSpeedFactor, we adjust the fan speed
        double minTime = (getSettingInSeconds("cool_min_layer_time"));
        double maxTime = (getSettingInSeconds("cool_min_layer_time_fan_speed_max"));
        double fanSpeedMin = getSettingInPercentage("cool_fan_speed_min");
        double fanSpeedMax = getSettingInPercentage("cool_fan_speed_max");
        fanSpeed = fanSpeedMax - (fanSpeedMax-fanSpeedMin) * (totalLayerTime - minTime) / (maxTime - minTime);
    }
    if (static_cast<int>(layer_nr) < getSettingAsCount("cool_fan_full_layer"))
    {
        //Slow down the fan on the layers below the [cool_fan_full_layer], where layer 0 is speed 0.
        fanSpeed = fanSpeed * layer_nr / getSettingAsCount("cool_fan_full_layer");
    }
    gcode.writeFanCommand(fanSpeed);
}

void FffGcodeWriter::finalize()
{
    gcode.finalize(max_object_height, getSettingInMillimetersPerSecond("speed_travel"), getSettingString("machine_end_gcode").c_str());
    for(int e=0; e<MAX_EXTRUDERS; e++)
        gcode.writeTemperatureCommand(e, 0, false);
}


} // namespace cura
