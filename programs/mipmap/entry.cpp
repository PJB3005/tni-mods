#include "../../src/image/mipmap_gen.hpp"

#include <api.hpp>

#include "tower.hpp"

#include <cstdlib>

/*
TNI modding template

https://libriscv.no/docs/host_langs/godot_integration/godot_docs/cppexamples/
https://libriscv.no/docs/host_langs/godot_integration/godot_docs/cppapi

*/

using namespace std::string_view_literals;

EXTERN_SYSCALL(uint64_t, sys_node_create, Node_Create_Shortlist, const char *, size_t, const char *, size_t);

static Mod mod = Mod(0);
static ModApiV1 api_v1 = ModApiV1(0);

static Dictionary converted_texture_map;

// This should be called at the entrance of any calls into the mod.
// eg, callbacks like `on_mod_load` and any callables given to Godot.
inline void fetch_mod_api() {
	mod = get_node<Mod>();
	api_v1 = mod.api_v1();
}

static int somevar;

static char const* GodotFormatName(int64_t format) {
    switch (format) {
    case Image::FORMAT_RGBA8:
        return "RGBA8";
    case Image::FORMAT_RGB8:
        return "RGB8";
    case Image::FORMAT_DXT1:
        return "DXT1";
    case Image::FORMAT_DXT3:
        return "DXT3";
    case Image::FORMAT_DXT5:
        return "DXT7";
    case Image::FORMAT_BPTC_RGBA:
        return "BPTC_RGBA";
    default:
        return "Unknown";
    }
}

void print_scenario_name() {
	// sample function to demonstrate usage of the game API
	std::optional<GameWorld> gw = api_v1.get_game_world();
	if (!gw) return;
	
    String scenario_name = gw.value().scenario_name();
    printf("scenario %s\n", scenario_name.utf8().c_str());
}

static Variant on_mod_load() {
	fetch_mod_api();
	printf("Loaded experiments\n");

    Variant ctm = Dictionary::Create();
    ctm.make_permanent();
    converted_texture_map = ctm;

	return Nil;
}

static experiments::ImageFormat FromGodotFormat(int64_t format) {
    switch (format) {
    case Image::FORMAT_RGBA8:
        return experiments::ImageFormat::RGBA8;
    case Image::FORMAT_RGB8:
        return experiments::ImageFormat::RGB8;
    default:
        throw std::runtime_error("Unknown Godot format!");
    }
}

static void better_gen_mips(Image img) {
    img.clear_mipmaps();

    auto data_arr = img.get_data();
    auto data = data_arr.fetch();

    experiments::LoadedImageBuffer buf {
        .width = static_cast<uint32_t>(img.get_width()),
        .height = static_cast<uint32_t>(img.get_height()),
        .mipLevels = 1,
        .format = FromGodotFormat(img.get_format()),
        .data = {
            std::move(data)
        }
    };

    buf = experiments::genMipmaps(std::move(buf));

    // print("Image size: ", buf.width, "x", buf.height, "\n");

    size_t final_size = 0;
    for (auto const& level : buf.data) {
        // print("Level: ", level.size(), "\n");
        final_size += level.size();
    }

    std::vector<unsigned char> final_vec;
    final_vec.resize(final_size);

    size_t offset = 0;
    for (auto const& level : buf.data) {
        std::memcpy(&final_vec[offset], level.data(), level.size());
        offset += level.size();
    }

    data_arr.store(final_vec);
    img.set_data(buf.width, buf.height, true, img.get_format(), data_arr);
}

static std::optional<Texture2D> FixTexture(Texture2D texture) {
    if (texture.is_class("CanvasTexture")) {
        auto copy = cast_to<CanvasTexture>(texture.duplicate());
        auto diffuse = copy.get("diffuse_texture");
        if (diffuse.is_nil()) {
            return std::nullopt;
        }

        auto replacement = FixTexture(cast_to<Texture2D>(diffuse));
        if (!replacement.has_value()) {
            return std::nullopt;
        }

        copy.set_diffuse_texture(*replacement);
        return copy;
    }

    auto path = texture.get_path();
    auto found = converted_texture_map.get(path);
    if (!found.is_nil()) {
        return cast_to<Texture2D>(found);
    }

    std::printf("Generating mips for %s...\n", path.utf8().c_str());

    auto image = cast_to<Image>(texture.get_image());
    auto const format = image.get_format();
    if (format != Image::FORMAT_RGBA8 && format != Image::FORMAT_RGB8) {
        std::printf("Skipping as not RGB(A)8 format! Was %s\n", GodotFormatName(format));
        return texture;
    }

    better_gen_mips(image);

    auto imageTex = ClassDB::instantiate<ImageTexture>("ImageTexture");
    imageTex.set_image(image);

    converted_texture_map[path] = imageTex;

    return imageTex;
}

static void FixSprite(Sprite2D sprite) {
    if (sprite.get_texture_filter() >= CanvasItem::TEXTURE_FILTER_LINEAR) {
        return;
    }

    sprite.set_texture_filter(CanvasItem::TEXTURE_FILTER_LINEAR_WITH_MIPMAPS);
    // sprite.set_self_modulate(RED);

    auto tex_var = sprite.get("texture");
    if (tex_var.is_nil()) {
        return;
    }

    auto texture = cast_to<Texture2D>(tex_var);
    auto replacement = FixTexture(texture);
    if (!replacement.has_value()) {
        return;
    }

    sprite.set_texture(*replacement);
}

static void FixNode(Node object) {
    for (auto child : object.get_children()) {
        if (child.get_class() == "Sprite2D") {
            FixSprite(cast_to<Sprite2D>(child));
            continue;
        }

        auto childName = child.get_name().as_std_string();
        if (
                // LEDs on various devices
                childName.starts_with("SquareLED_A")
                // Switches and  toggles
                || childName == "Toggle_A" || childName == "Push_B" || childName == "Push_A" || childName == "Red_B"
                // Button on the cable-o-matic
                || childName == "SpoolButton"
                // Trolley
                || childName == "ConfigurationA") {
            std::printf("The thing\n");
            FixNode(child);
        }
    }
}

static void FixGraphController(GraphController controller) {
    Variant ports = controller.ports();

    for (auto port : ports.as_array()) {
        if (port.get_type() == Variant::Type::OBJECT) {
            auto node = cast_to<Node>(port);
            // std::printf("Port '%s'\n", node.get_name().as_std_string().c_str());
            FixNode(node);

            auto lock_switch = node.get("peripheral_lock_switch");
            if (!lock_switch.is_nil()) {
                FixNode(lock_switch);
            }
        }

    }
}

static void FixDeviceOrFixture(DeviceUnit object) {
    FixNode(object);

    // std::printf("Device %s: ", object.get_name().as_std_string().c_str());

    auto lc_var = object.get("logic_controller");
    if (!lc_var.is_nil()) {
        FixGraphController(lc_var);
    }

    //

    auto pc_var = object.get("power_controller");
    if (!pc_var.is_nil()) {
        FixGraphController(pc_var);
    }

    // std::printf("\n");
}



static void FixLocation(Location location) {
    std::printf("Location: %s\n", location.get_name().as_std_string().c_str());

    auto power_var = location.get("power_mains");
    if (!power_var.is_nil()) {
        FixGraphController(power_var);
    }

    auto power_meter = location.get("power_meter");
    if (!power_meter.is_nil()) {
        FixDeviceOrFixture(power_meter);
    }

    for (auto child : location.get_children()) {
        FixNode(child);
        // Check if outlet.
        auto sockets = child.get("sockets");
        if (!sockets.is_nil()) {
            for (auto socket : sockets.as_array()) {
                FixNode(socket);
            }
        }
    }
}

static void FixAllLocations() {
    auto gameWorld = api_v1.get_game_world();
    if (!gameWorld.has_value()) {
        return;
    }

    Variant locations = gameWorld->locations();
    for (auto location : locations.as_array()) {
        FixLocation(location);
    }

    Variant fixtures = gameWorld->get("fixture_nodes");
    for (auto child : cast_to<Node2D>(fixtures).get_children()) {
        FixNode(child);
    }

    Variant components = gameWorld->get("component_nodes");
    for (auto child : cast_to<Node2D>(components).get_children()) {
        FixNode(child);
    }
}

static Variant on_mods_loaded() {
	fetch_mod_api();
	printf("All mods have been loaded (or have they?)...\n");

	printf("Game version: %s\n", api_v1.get_game_version().to_version_string().utf8().c_str());

    auto devices = api_v1.get_devices();
    for (auto unit : devices.as_array()) {
        auto device = cast_to<DeviceUnit>(unit);
        FixDeviceOrFixture(device);
    }

    FixAllLocations();

	return Nil;
}

static Variant on_game_tick(double delta) {
	fetch_mod_api();
	// this is called every game tick.
	return Nil;
}

static Variant on_player_input(InputEvent event) {
	fetch_mod_api();
	// this is called when player input is received.
    if (event.get_class() == "InputEventKey") {
        InputEventKey* key = reinterpret_cast<InputEventKey*>(&event);
        
        if (key->is_pressed() && key->get_keycode() == 32) {
            printf("SPACEEEE! %d\n", somevar);

            auto world = api_v1.get_game_world();
            if (!world.has_value()) {
                return {};
            }

            MobileOSLayer mobile_os = world->mobile_os_cvl();
            Screen screen = mobile_os.screen();
            print(screen.get("main_control"sv));
        }
    }
	
	return Nil;
}

static Variant on_game_state_ready() {
	fetch_mod_api();
	// this is called when the game is ready.
    FixAllLocations();
	return Nil;
}

static Variant on_game_host_eod() {
	fetch_mod_api();
	// this is called when the game host's end-of-day occurs.
	return Nil;
}

static Variant on_device_spawned(DeviceUnit device) {
	fetch_mod_api();
    FixDeviceOrFixture(device);
    // this is called when a device is spawned.
	if ((int64_t)device.device_hardware_class() == (int64_t)DeviceUnit::DeviceHardwareClass::NETWORK_SWITCH) {
		LogicController lc = device.logic_controller();
		int64_t original_nbw = lc.installed_nbw();
		int64_t new_nbw = original_nbw * 2;
		lc.installed_nbw() = new_nbw;
        printf("modified bandwidth of %s from %ld to %ld\n", ((String)device.product_name()).utf8().c_str(), original_nbw, new_nbw);
	}
    return Nil;
}

int main() {
    // stdout line buffering, to match TNI mod output buffering.
    setvbuf(stdout, NULL, _IOLBF, BUFSIZ);

	// These are the various optional callbacks that TNI provides to modders.
	ADD_API_FUNCTION(on_mod_load, "", "", "Called immediately after the mod is loaded (after main())");
	ADD_API_FUNCTION(on_mods_loaded, "", "", "Called when all mods have been loaded");
	ADD_API_FUNCTION(on_game_state_ready, "", "", "Called when the game is ready");
	ADD_API_FUNCTION(on_game_host_eod, "", "", "Called when the end-of-day occurs on the host");
	ADD_API_FUNCTION(on_game_tick, "", "", "Called every game tick");
	ADD_API_FUNCTION(on_player_input, "", "", "Called when player input");
	ADD_API_FUNCTION(on_device_spawned, "", "", "Called when device spawned");

	halt();
}
