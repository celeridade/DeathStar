#include "Mapper.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <cstring>

static std::unordered_map<std::string, std::string> s_ClassMappings;
static std::unordered_map<std::string, std::string> s_FieldMappings;
static std::unordered_map<std::string, std::string> s_MethodMappings;
static GameVersions s_CurrentVersion = UNKNOWN;

static std::vector<std::pair<const char*, const char*>> s_VanillaFieldMappings;
static std::vector<std::pair<const char*, const char*>> s_VanillaMethodMappings;
static std::vector<std::pair<const char*, const char*>> s_SrgFieldMappings;
static std::vector<std::pair<const char*, const char*>> s_SrgMethodMappings;
static std::vector<std::pair<const char*, const char*>> s_LunarFieldMappings;
static std::vector<std::pair<const char*, const char*>> s_LunarMethodMappings;
static std::vector<std::pair<const char*, const char*>> s_BadlionClassMappings;
static std::vector<std::pair<const char*, const char*>> s_BadlionFieldMappings;
static std::vector<std::pair<const char*, const char*>> s_BadlionMethodMappings;

static void CreateVanillaMappings()
{
    s_ClassMappings.clear();
    s_FieldMappings.clear();
    s_MethodMappings.clear();

    s_ClassMappings["Minecraft"] = "net/minecraft/client/Minecraft";
    s_ClassMappings["EntityPlayerSP"] = "net/minecraft/client/entity/EntityPlayerSP";
    s_ClassMappings["WorldClient"] = "net/minecraft/client/multiplayer/WorldClient";
    s_ClassMappings["GuiScreen"] = "net/minecraft/client/gui/GuiScreen";
    s_ClassMappings["PlayerControllerMP"] = "net/minecraft/client/multiplayer/PlayerControllerMP";
    s_ClassMappings["MovingObjectPosition"] = "net/minecraft/util/MovingObjectPosition";
    s_ClassMappings["EntityLivingBase"] = "net/minecraft/entity/EntityLivingBase";
    s_ClassMappings["Entity"] = "net/minecraft/entity/Entity";
    s_ClassMappings["EntityPlayer"] = "net/minecraft/entity/player/EntityPlayer";
    s_ClassMappings["GuiIngame"] = "net/minecraft/client/gui/GuiIngame";
    s_ClassMappings["Session"] = "net/minecraft/util/Session";
    s_ClassMappings["GuiInventory"] = "net/minecraft/client/gui/inventory/GuiInventory";
    s_ClassMappings["GuiContainer"] = "net/minecraft/client/gui/inventory/GuiContainer";
    s_ClassMappings["FontRenderer"] = "net/minecraft/client/gui/FontRenderer";
    s_ClassMappings["RenderManager"] = "net/minecraft/client/renderer/entity/RenderManager";
    s_ClassMappings["EntityRenderer"] = "net/minecraft/client/renderer/EntityRenderer";
    s_ClassMappings["GameSettings"] = "net/minecraft/client/settings/GameSettings";
    s_ClassMappings["NetHandlerPlayClient"] = "net/minecraft/client/network/NetHandlerPlayClient";
    s_ClassMappings["NetworkManager"] = "net/minecraft/network/NetworkManager";
    s_ClassMappings["BlockPos"] = "net/minecraft/util/BlockPos";
    s_ClassMappings["EnumFacing"] = "net/minecraft/util/EnumFacing";
    s_ClassMappings["Vec3"] = "net/minecraft/util/Vec3";
    s_ClassMappings["ItemStack"] = "net/minecraft/item/ItemStack";
    s_ClassMappings["Item"] = "net/minecraft/item/Item";
    s_ClassMappings["World"] = "net/minecraft/world/World";
    s_ClassMappings["EntityOtherPlayerMP"] = "net/minecraft/client/entity/EntityOtherPlayerMP";
    s_ClassMappings["EntityRenderer2"] = "net/minecraft/client/renderer/EntityRenderer2";
    s_ClassMappings["MouseFilter"] = "net/minecraft/util/MouseFilter";

    s_VanillaFieldMappings = {
        {"thePlayer", "thePlayer"},
        {"theWorld", "theWorld"},
        {"currentScreen", "currentScreen"},
        {"playerController", "playerController"},
        {"objectMouseOver", "objectMouseOver"},
        {"leftClickCounter", "leftClickCounter"},
        {"rightClickCounter", "rightClickCounter"},
        {"timer", "timer"},
        {"pointedEntity", "pointedEntity"},
        {"renderGlobal", "renderGlobal"},
        {"renderManager", "renderManager"},
        {"entityRenderer", "entityRenderer"},
        {"fontRenderer", "fontRenderer"},
        {"gameSettings", "gameSettings"},
        {"netHandler", "netHandler"},
        {"session", "session"},
        {"mcProfiler", "mcProfiler"},
        {"fullScreen", "fullScreen"},
        {"isGamePaused", "isGamePaused"},
        {"running", "running"},
        {"jumpTicks", "jumpTicks"},
        {"hurtTime", "hurtTime"},
        {"maxHurtTime", "maxHurtTime"},
        {"hurtResistantTime", "hurtResistantTime"},
        {"maxHurtResistantTime", "maxHurtResistantTime"},
        {"deathTime", "deathTime"},
        {"motionX", "motionX"},
        {"motionY", "motionY"},
        {"motionZ", "motionZ"},
        {"onGround", "onGround"},
        {"isSwingInProgress", "isSwingInProgress"},
        {"swingProgressInt", "swingProgressInt"},
        {"swingProgress", "swingProgress"},
        {"isAirBorne", "isAirBorne"},
        {"prevRotationYaw", "prevRotationYaw"},
        {"prevRotationPitch", "prevRotationPitch"},
        {"rotationYaw", "rotationYaw"},
        {"rotationPitch", "rotationPitch"},
        {"prevPosX", "prevPosX"},
        {"prevPosY", "prevPosY"},
        {"prevPosZ", "prevPosZ"},
        {"posX", "posX"},
        {"posY", "posY"},
        {"posZ", "posZ"},
        {"lastTickPosX", "lastTickPosX"},
        {"lastTickPosY", "lastTickPosY"},
        {"lastTickPosZ", "lastTickPosZ"},
        {"width", "width"},
        {"height", "height"},
        {"overlayTerrain", "overlayTerrain"},
        {"bossBar", "bossBar"},
        {"recordPlaying", "recordPlaying"},
        {"tabList", "tabList"},
        {"viewerPosX", "viewerPosX"},
        {"viewerPosY", "viewerPosY"},
        {"viewerPosZ", "viewerPosZ"},
        {"itemX", "itemY"},
        {"itemY", "itemY"},
        {"itemZ", "itemZ"},
        {"blockPos", "blockPos"},
        {"typeOfHit", "typeOfHit"},
        {"entityHit", "entityHit"},
        {"hitVec", "hitVec"},
        {"sideHit", "sideHit"},
        {"inventory", "inventory"},
        {"mainInventory", "mainInventory"},
        {"armorInventory", "armorInventory"},
        {"currentItem", "currentItem"},
        {"inventorySlots", "inventorySlots"},
        {"openContainer", "openContainer"},
        {"serverBrand", "serverBrand"},
        {"itemInUse", "itemInUse"},
        {"itemInUseCount", "itemInUseCount"},
        {"isBlocking", "isBlocking"},
        {"isEating", "isEating"},
        {"isDrinking", "isDrinking"},
        {"movementYaw", "movementYaw"},
        {"movementPitch", "movementPitch"},
        {"movementRoll", "movementRoll"},
        {"cameraYaw", "cameraYaw"},
        {"cameraPitch", "cameraPitch"},
        {"prevCameraYaw", "prevCameraYaw"},
        {"prevCameraPitch", "prevCameraPitch"},
        {"fov", "fov"},
        {"fovOffset", "fovOffset"},
        {"distance", "distance"},
        {"partialTicks", "partialTicks"},
        {"renderStartX", "renderStartX"},
        {"renderStartY", "renderStartY"},
        {"renderStartZ", "renderStartZ"},
        {"playerViewX", "playerViewX"},
        {"playerViewY", "playerViewY"},
        {"farPlaneDistance", "farPlaneDistance"},
        {"displayWidth", "displayWidth"},
        {"displayHeight", "displayHeight"},
        {"mouseFilterXAxis", "mouseFilterXAxis"},
        {"mouseFilterYAxis", "mouseFilterYAxis"},
        {"playerEntities", "playerEntities"},
    };

    s_VanillaMethodMappings = {
        {"clickMouse", "clickMouse"},
        {"rightClickMouse", "rightClickMouse"},
        {"middleClickMouse", "middleClickMouse"},
        {"launchIntegratedServer", "launchIntegratedServer"},
        {"startGame", "startGame"},
        {"shutdown", "shutdown"},
        {"resize", "resize"},
        {"createDisplay", "createDisplay"},
        {"displayGuiScreen", "displayGuiScreen"},
        {"isFullScreen", "isFullScreen"},
        {"toggleFullScreen", "toggleFullScreen"},
        {"refreshResources", "refreshResources"},
        {"getMinecraft", "getMinecraft"},
        {"getSession", "getSession"},
        {"getFontRenderer", "getFontRenderer"},
        {"isFancyGraphicsEnabled", "isFancyGraphicsEnabled"},
        {"isAmbientOcclusionEnabled", "isAmbientOcclusionEnabled"},
        {"isFramerateLimit", "isFramerateLimit"},
        {"isFramerateLimitEnabled", "isFramerateLimitEnabled"},
        {"isIntegratedServerRunning", "isIntegratedServerRunning"},
        {"isSingleplayer", "isSingleplayer"},
        {"isGamePaused", "isGamePaused"},
        {"getCurrentServerData", "getCurrentServerData"},
        {"getNetHandler", "getNetHandler"},
        {"getRenderGlobal", "getRenderGlobal"},
        {"getRenderViewEntity", "getRenderViewEntity"},
        {"setRenderViewEntity", "setRenderViewEntity"},
        {"getSoundHandler", "getSoundHandler"},
        {"getLanguageManager", "getLanguageManager"},
        {"getTextureManager", "getTextureManager"},
        {"getResourceManager", "getResourceManager"},
        {"refreshResources", "refreshResources"},
        {"isSneaking", "isSneaking"},
        {"isSprinting", "isSprinting"},
        {"isUsingItem", "isUsingItem"},
        {"isRiding", "isRiding"},
        {"isInWater", "isInWater"},
        {"isBurning", "isBurning"},
        {"isInLava", "isInLava"},
        {"isOnLadder", "isOnLadder"},
        {"isAirBorne", "isAirBorne"},
        {"isSwingInProgress", "isSwingInProgress"},
        {"isEating", "isEating"},
        {"isDrinking", "isDrinking"},
        {"isBlocking", "isBlocking"},
        {"isDead", "isDead"},
        {"isEntityAlive", "isEntityAlive"},
        {"isEntityInsideOpaqueBlock", "isEntityInsideOpaqueBlock"},
        {"isWet", "isWet"},
        {"isPushedByWater", "isPushedByWater"},
        {"isCollidedHorizontally", "isCollidedHorizontally"},
        {"isCollidedVertically", "isCollidedVertically"},
        {"isCollided", "isCollided"},
        {"isInvisible", "isInvisible"},
        {"isInvisibleToPlayer", "isInvisibleToPlayer"},
        {"isImmuneToFire", "isImmuneToFire"},
        {"isPotionActive", "isPotionActive"},
        {"isEntityUndead", "isEntityUndead"},
        {"addPotionEffect", "addPotionEffect"},
        {"removePotionEffect", "removePotionEffect"},
        {"getActivePotionEffect", "getActivePotionEffect"},
        {"getActivePotionEffects", "getActivePotionEffects"},
        {"setJumpTicks", "setJumpTicks"},
        {"setPosition", "setPosition"},
        {"setPositionAndRotation", "setPositionAndRotation"},
        {"setVelocity", "setVelocity"},
        {"setAngles", "setAngles"},
        {"attackTargetEntityWithCurrentItem", "attackTargetEntityWithCurrentItem"},
        {"sendClickBlockToController", "sendClickBlockToController"},
        {"clickBlock", "clickBlock"},
        {"clickBlockCreative", "clickBlockCreative"},
        {"onPlayerDamageBlock", "onPlayerDamageBlock"},
        {"resetBlockRemoving", "resetBlockRemoving"},
        {"getBlockPos", "getBlockPos"},
        {"isOnSameTeam", "isOnSameTeam"},
    };

    for (const auto& pair : s_VanillaFieldMappings)
        s_FieldMappings[pair.first] = pair.second;
    for (const auto& pair : s_VanillaMethodMappings)
        s_MethodMappings[pair.first] = pair.second;
}

static void ApplySrgMappings()
{
    s_SrgFieldMappings = {
        {"thePlayer", "field_71439_y"},
        {"theWorld", "field_71441_e"},
        {"currentScreen", "field_71462_r"},
        {"playerController", "field_71456_v"},
        {"objectMouseOver", "field_71476_x"},
        {"leftClickCounter", "field_71429_W"},
        {"rightClickCounter", "field_71449_j"},
        {"timer", "field_71428_T"},
        {"pointedEntity", "field_71444_c"},
        {"renderGlobal", "field_71461_s"},
        {"renderManager", "field_72463_p"},
        {"entityRenderer", "field_71460_t"},
        {"fontRenderer", "field_71466_p"},
        {"gameSettings", "field_71474_y"},
        {"netHandler", "field_71451_h"},
        {"session", "field_71458_q"},
        {"mcProfiler", "field_71424_I"},
        {"fullScreen", "field_71416_A"},
        {"isGamePaused", "field_71421_N"},
        {"running", "field_71425_Q"},
        {"jumpTicks", "field_70773_bE"},
        {"hurtTime", "field_70737_aN"},
        {"maxHurtTime", "field_70738_aO"},
        {"hurtResistantTime", "field_70170_p"},
        {"maxHurtResistantTime", "field_70772_bj"},
        {"deathTime", "field_70770_ap"},
        {"motionX", "field_70159_w"},
        {"motionY", "field_70181_x"},
        {"motionZ", "field_70179_y"},
        {"onGround", "field_70122_E"},
        {"isSwingInProgress", "field_184622_c"},
        {"swingProgressInt", "field_70702_br"},
        {"swingProgress", "field_70701_bs"},
        {"isAirBorne", "field_70153_n"},
        {"prevRotationYaw", "field_70126_B"},
        {"prevRotationPitch", "field_70127_C"},
        {"rotationYaw", "field_70177_z"},
        {"rotationPitch", "field_70125_A"},
        {"prevPosX", "field_70165_t"},
        {"prevPosY", "field_70161_u"},
        {"prevPosZ", "field_70166_v"},
        {"posX", "field_70165_t"},
        {"posY", "field_70161_u"},
        {"posZ", "field_70166_v"},
        {"lastTickPosX", "field_70169_q"},
        {"lastTickPosY", "field_70167_r"},
        {"lastTickPosZ", "field_70168_s"},
        {"width", "field_70130_N"},
        {"height", "field_70131_O"},
        {"inventory", "field_71071_by"},
        {"mainInventory", "field_70460_b"},
        {"armorInventory", "field_70461_c"},
        {"currentItem", "field_70461_c"},
        {"inventorySlots", "field_75151_b"},
        {"openContainer", "field_71070_bA"},
        {"itemInUse", "field_71074_bz"},
        {"itemInUseCount", "field_71072_bA"},
        {"isBlocking", "field_71073_bB"},
        {"isEating", "field_71076_bD"},
        {"isDrinking", "field_71075_bE"},
        {"serverBrand", "field_71147_b"},
        {"blockPos", "field_174879_c"},
        {"typeOfHit", "field_178784_b"},
        {"entityHit", "field_72308_d"},
        {"hitVec", "field_72307_f"},
        {"sideHit", "field_178785_b"},
        {"movementYaw", "field_181012_aH"},
        {"cameraYaw", "field_70474_R"},
        {"cameraPitch", "field_70473_S"},
        {"prevCameraYaw", "field_70469_T"},
        {"prevCameraPitch", "field_70468_U"},
        {"distance", "field_78730_l"},
        {"partialTicks", "field_78725_b"},
        {"renderStartX", "field_78729_p"},
        {"renderStartY", "field_78728_q"},
        {"renderStartZ", "field_78727_r"},
        {"playerViewX", "field_78725_b"},
        {"playerViewY", "field_78726_c"},
        {"farPlaneDistance", "field_78733_x"},
        {"displayWidth", "field_71448_r"},
        {"displayHeight", "field_71447_s"},
        {"mouseFilterXAxis", "field_71778_A"},
        {"mouseFilterYAxis", "field_71777_B"},
        {"playerEntities", "field_73010_i"},
    };

    s_SrgMethodMappings = {
        {"clickMouse", "func_147116_af"},
        {"rightClickMouse", "func_147121_ag"},
        {"middleClickMouse", "func_147112_ai"},
        {"launchIntegratedServer", "func_71377_b"},
        {"startGame", "func_71384_a"},
        {"shutdown", "func_71405_g"},
        {"resize", "func_71370_a"},
        {"createDisplay", "func_71373_a"},
        {"displayGuiScreen", "func_147108_a"},
        {"isFullScreen", "func_71415_C"},
        {"toggleFullScreen", "func_71407_B"},
        {"refreshResources", "func_110436_a"},
        {"getMinecraft", "func_71410_x"},
        {"getSession", "func_71401_C"},
        {"getFontRenderer", "func_71412_D"},
        {"attackTargetEntityWithCurrentItem", "func_78764_a"},
        {"sendClickBlockToController", "func_147115_a"},
        {"clickBlock", "func_147116_af"},
        {"clickBlockCreative", "func_147113_a"},
        {"onPlayerDamageBlock", "func_147107_a"},
        {"resetBlockRemoving", "func_147112_ai"},
        {"isSneaking", "func_70093_af"},
        {"isSprinting", "func_70051_ag"},
        {"isUsingItem", "func_71080_bA"},
        {"isRiding", "func_70626_be"},
        {"isInWater", "func_70090_N"},
        {"isBurning", "func_70027_ad"},
        {"isInLava", "func_70090_N"},
        {"isOnLadder", "func_70617_f"},
        {"isAirBorne", "func_70053_U"},
        {"isDead", "func_70093_af"},
        {"isEntityAlive", "func_70089_S"},
        {"setJumpTicks", "func_70749_aE"},
        {"setPosition", "func_70107_b"},
        {"setPositionAndRotation", "func_70106_y"},
        {"setVelocity", "func_70016_h"},
        {"setAngles", "func_70082_b"},
        {"getBlockPos", "func_174894_a"},
        {"isOnSameTeam", "func_142054_a"},
    };

    for (const auto& pair : s_SrgFieldMappings)
        s_FieldMappings[pair.first] = pair.second;
    for (const auto& pair : s_SrgMethodMappings)
        s_MethodMappings[pair.first] = pair.second;
}

static void ApplyLunarMappings()
{
    s_LunarFieldMappings = {
        {"thePlayer", "thePlayer"},
        {"theWorld", "theWorld"},
        {"currentScreen", "currentScreen"},
        {"playerController", "playerController"},
        {"objectMouseOver", "objectMouseOver"},
        {"leftClickCounter", "leftClickCounter"},
        {"rightClickCounter", "rightClickCounter"},
        {"timer", "timer"},
        {"jumpTicks", "jumpTicks"},
        {"hurtTime", "hurtTime"},
        {"maxHurtTime", "maxHurtTime"},
        {"hurtResistantTime", "hurtResistantTime"},
        {"maxHurtResistantTime", "maxHurtResistantTime"},
        {"deathTime", "deathTime"},
        {"motionX", "motionX"},
        {"motionY", "motionY"},
        {"motionZ", "motionZ"},
        {"onGround", "onGround"},
        {"renderGlobal", "renderGlobal"},
        {"renderManager", "renderManager"},
        {"entityRenderer", "entityRenderer"},
        {"playerEntities", "playerEntities"},
    };

    s_LunarMethodMappings = {
        {"clickMouse", "clickMouse"},
        {"rightClickMouse", "rightClickMouse"},
        {"attackTargetEntityWithCurrentItem", "attackTargetEntityWithCurrentItem"},
        {"sendClickBlockToController", "sendClickBlockToController"},
        {"displayGuiScreen", "displayGuiScreen"},
        {"getMinecraft", "getMinecraft"},
        {"isOnSameTeam", "isOnSameTeam"},
    };

    for (const auto& pair : s_LunarFieldMappings)
        s_FieldMappings[pair.first] = pair.second;
    for (const auto& pair : s_LunarMethodMappings)
        s_MethodMappings[pair.first] = pair.second;
}

static void ApplyBadlionMappings()
{
    s_BadlionClassMappings = {
        {"Minecraft", "ave"},
        {"EntityPlayerSP", "blk"},
        {"WorldClient", "bqz"},
        {"GuiScreen", "bci"},
        {"PlayerControllerMP", "bnz"},
        {"MovingObjectPosition", "bhb"},
        {"EntityLivingBase", "pr"},
        {"Entity", "pk"},
        {"EntityPlayer", "po"},
        {"GuiIngame", "bjd"},
        {"Session", "aob"},
        {"GuiInventory", "bld"},
        {"GuiContainer", "bkr"},
        {"FontRenderer", "aud"},
        {"RenderManager", "bph"},
        {"EntityRenderer", "bmp"},
        {"GameSettings", "biw"},
        {"NetHandlerPlayClient", "bnw"},
        {"NetworkManager", "gx"},
        {"BlockPos", "cj"},
        {"EnumFacing", "cq"},
        {"Vec3", "csa"},
        {"ItemStack", "ace"},
        {"Item", "abp"},
        {"World", "bo"},
        {"EntityOtherPlayerMP", "blm"},
        {"MouseFilter", "ase"},
    };

    s_BadlionFieldMappings = {
        {"thePlayer", "h"},
        {"theWorld", "f"},
        {"currentScreen", "m"},
        {"playerController", "o"},
        {"objectMouseOver", "v"},
        {"leftClickCounter", "at"},
        {"rightClickCounter", "au"},
        {"timer", "ah"},
        {"pointedEntity", "c"},
        {"renderGlobal", "s"},
        {"renderManager", "m"},
        {"entityRenderer", "t"},
        {"fontRenderer", "p"},
        {"gameSettings", "x"},
        {"netHandler", "h"},
        {"session", "q"},
        {"mcProfiler", "F"},
        {"fullScreen", "v"},
        {"isGamePaused", "B"},
        {"running", "C"},
        {"jumpTicks", "ba"},
        {"hurtTime", "bt"},
        {"maxHurtTime", "bu"},
        {"hurtResistantTime", "S"},
        {"maxHurtResistantTime", "bx"},
        {"deathTime", "bs"},
        {"motionX", "v"},
        {"motionY", "w"},
        {"motionZ", "x"},
        {"onGround", "y"},
        {"isSwingInProgress", "ax"},
        {"swingProgressInt", "aA"},
        {"swingProgress", "az"},
        {"isAirBorne", "P"},
        {"prevRotationYaw", "A"},
        {"prevRotationPitch", "B"},
        {"rotationYaw", "z"},
        {"rotationPitch", "A"},
        {"prevPosX", "t"},
        {"prevPosY", "u"},
        {"prevPosZ", "v"},
        {"posX", "t"},
        {"posY", "u"},
        {"posZ", "v"},
        {"lastTickPosX", "q"},
        {"lastTickPosY", "r"},
        {"lastTickPosZ", "s"},
        {"width", "M"},
        {"height", "N"},
        {"inventory", "by"},
        {"mainInventory", "b"},
        {"armorInventory", "c"},
        {"currentItem", "c"},
        {"inventorySlots", "b"},
        {"openContainer", "bA"},
        {"itemInUse", "bz"},
        {"itemInUseCount", "bA"},
        {"isBlocking", "bB"},
        {"isEating", "bD"},
        {"isDrinking", "bE"},
        {"serverBrand", "b"},
        {"blockPos", "c"},
        {"typeOfHit", "b"},
        {"entityHit", "d"},
        {"hitVec", "f"},
        {"sideHit", "b"},
        {"movementYaw", "aH"},
        {"cameraYaw", "R"},
        {"cameraPitch", "S"},
        {"prevCameraYaw", "T"},
        {"prevCameraPitch", "U"},
        {"distance", "l"},
        {"partialTicks", "b"},
        {"renderStartX", "p"},
        {"renderStartY", "q"},
        {"renderStartZ", "r"},
        {"playerViewX", "b"},
        {"playerViewY", "c"},
        {"farPlaneDistance", "x"},
        {"displayWidth", "r"},
        {"displayHeight", "s"},
        {"mouseFilterXAxis", "A"},
        {"mouseFilterYAxis", "B"},
        {"playerEntities", "i"},
    };

    s_BadlionMethodMappings = {
        {"clickMouse", "N"},
        {"rightClickMouse", "P"},
        {"middleClickMouse", "Q"},
        {"launchIntegratedServer", "b"},
        {"startGame", "a"},
        {"shutdown", "g"},
        {"resize", "a"},
        {"createDisplay", "a"},
        {"displayGuiScreen", "a"},
        {"isFullScreen", "C"},
        {"toggleFullScreen", "B"},
        {"refreshResources", "a"},
        {"getMinecraft", "x"},
        {"getSession", "C"},
        {"getFontRenderer", "D"},
        {"attackTargetEntityWithCurrentItem", "a"},
        {"sendClickBlockToController", "a"},
        {"clickBlock", "a"},
        {"clickBlockCreative", "a"},
        {"onPlayerDamageBlock", "a"},
        {"resetBlockRemoving", "a"},
        {"isSneaking", "af"},
        {"isSprinting", "ag"},
        {"isUsingItem", "bA"},
        {"isRiding", "be"},
        {"isInWater", "N"},
        {"isBurning", "ad"},
        {"isOnLadder", "f"},
        {"isDead", "af"},
        {"isEntityAlive", "S"},
        {"setJumpTicks", "aE"},
        {"setPosition", "b"},
        {"setPositionAndRotation", "y"},
        {"setVelocity", "h"},
        {"setAngles", "b"},
        {"getBlockPos", "a"},
        {"isOnSameTeam", "a"},
    };

    for (const auto& pair : s_BadlionClassMappings)
        s_ClassMappings[pair.first] = pair.second;
    for (const auto& pair : s_BadlionFieldMappings)
        s_FieldMappings[pair.first] = pair.second;
    for (const auto& pair : s_BadlionMethodMappings)
        s_MethodMappings[pair.first] = pair.second;
}

// CheatBreaker 1.8.9 runs its own Allatori pass, so neither the vanilla names
// nor the Badlion single letters exist. These are the obfuscated names for the
// handful of members the features touch.
static void ApplyCheatBreakerMappings()
{
    s_ClassMappings["Minecraft"] = "IllIIIlIlllllIllllIIIIIIl";
    s_ClassMappings["EntityClientPlayerMP"] = "IllIIIllIlIIlllIIIIllIlIl";
    s_ClassMappings["EntityPlayerSP"] = "IlllllllIIIIIIIIllIlIIIll";
    s_ClassMappings["EntityPlayer"] = "IllllIIlIIlIlIIlIlIlllllI";
    s_ClassMappings["EntityLivingBase"] = "IllIIIIllIlIllIlllIlIIIII";
    s_ClassMappings["Entity"] = "IllIIIlIlIIlIlIlIlllIlIII";
    s_ClassMappings["WorldClient"] = "IllIlIIIIllIlIllIIlllIIIl";
    s_ClassMappings["GuiScreen"] = "IlIIIlllIIIlIIIIllIIIIllI";
    s_ClassMappings["MovingObjectPosition"] = "IllllllIlIIlIllIlIllIIlII";
    s_ClassMappings["PlayerControllerMP"] = "IlIIIlIlIlIIIIlllIlIlIlll";

    s_FieldMappings["theMinecraft"] = "IllIIIIIIllllIIIIllIllIll";
    s_FieldMappings["thePlayer"] = "IllIIlIlIIlIIIIllIlIlIlll";
    s_FieldMappings["theWorld"] = "IlllIllllIIllllIllIlllIll";
    s_FieldMappings["currentScreen"] = "IlIIllIIlIlIllIIIIlIIIlIl";
    s_FieldMappings["leftClickCounter"] = "IlIIllllllllIllIllIIIlllI";
    s_FieldMappings["jumpTicks"] = "IllllIlIIIlIlllIllIIIIlII";
    s_FieldMappings["objectMouseOver"] = "IlIIIIIIllllIIllIlIIIllll";
    s_FieldMappings["typeOfHit"] = "IllllIllllllIIIlIlIIlIlIl";
    s_FieldMappings["playerController"] = "IllIIIIIIIIlIllIIIlllIIlI";
    s_FieldMappings["playerEntities"] = "IlIIIIlIlIllllIIlIllIlIlI";
    s_FieldMappings["isInvisible"] = "IllIIIIIIIIlIllIIIlllIIlI";
    s_FieldMappings["width"] = "IllIlllllIlIllIlllIllllll";
    s_FieldMappings["height"] = "IllIllIllIIlIlIlIlllIIllI";

    s_MethodMappings["getName"] = "IlIlIllIlIlIlllIlIIIIIllI";
    // The real account name comes from here; the entity's own getName returns
    // translation keys on this client.
    s_MethodMappings["getGameProfile"] = "IlIllIlIlIIlllllIlllIIlll";
}

void Mapper::Clear()
{
    auto scrub = [](std::unordered_map<std::string, std::string>& table) {
        for (auto& entry : table)
        {
            std::string& value = entry.second;
            for (size_t i = 0; i < value.size(); i++)
                value[i] = '\0';
        }
        table.clear();
    };

    scrub(s_ClassMappings);
    scrub(s_FieldMappings);
    scrub(s_MethodMappings);

    s_VanillaFieldMappings.clear();
    s_VanillaMethodMappings.clear();
    s_SrgFieldMappings.clear();
    s_SrgMethodMappings.clear();
    s_LunarFieldMappings.clear();
    s_LunarMethodMappings.clear();
    s_BadlionClassMappings.clear();
    s_BadlionFieldMappings.clear();
    s_BadlionMethodMappings.clear();

    s_CurrentVersion = UNKNOWN;
}

void Mapper::Initialize(GameVersions version)
{
    CreateVanillaMappings();

    switch (version)
    {
    case CHEATBREAKER:
        ApplyCheatBreakerMappings();
        break;
    case BADLION:
        ApplyBadlionMappings();
        break;
    case LUNAR:
        ApplyLunarMappings();
        break;
    case FEATHER_1_8:
    case FORGE_1_8:
        ApplySrgMappings();
        break;
    case UNKNOWN:
    default:
        break;
    }

    s_CurrentVersion = version;
}

std::string Mapper::Get(const char* mapping, int type)
{
    switch (type)
    {
    case CLASS_MAPPING:
    {
        auto it = s_ClassMappings.find(mapping);
        if (it != s_ClassMappings.end())
            return it->second;
        return mapping;
    }
    case FIELD_MAPPING:
    {
        auto it = s_FieldMappings.find(mapping);
        if (it != s_FieldMappings.end())
            return it->second;
        return mapping;
    }
    case METHOD_MAPPING:
    {
        auto it = s_MethodMappings.find(mapping);
        if (it != s_MethodMappings.end())
            return it->second;
        return mapping;
    }
    default:
        return mapping;
    }
}

void Mapper::Set(const char* key, const char* value)
{
    s_FieldMappings[key] = value;
}

void Mapper::SetMethod(const char* key, const char* value)
{
    s_MethodMappings[key] = value;
}

size_t Mapper::LoadBadlionMappingsFile(const wchar_t* path)
{
    std::wifstream file(path);
    if (!file.is_open())
        return 0;

    size_t count = 0;
    std::wstring line;
    while (std::getline(file, line))
    {
        if (line.empty() || line[0] == L'#')
            continue;

        std::wstringstream ss(line);
        std::wstring type, key, value;
        std::getline(ss, type, L':');
        std::getline(ss, key, L':');
        std::getline(ss, value, L':');

        if (type == L"class" || type == L"1")
        {
            std::string k(key.begin(), key.end());
            std::string v(value.begin(), value.end());
            s_ClassMappings[k] = v;
            count++;
        }
        else if (type == L"field" || type == L"2")
        {
            std::string k(key.begin(), key.end());
            std::string v(value.begin(), value.end());
            s_FieldMappings[k] = v;
            count++;
        }
        else if (type == L"method" || type == L"3")
        {
            std::string k(key.begin(), key.end());
            std::string v(value.begin(), value.end());
            s_MethodMappings[k] = v;
            count++;
        }
    }

    file.close();
    return count;
}
