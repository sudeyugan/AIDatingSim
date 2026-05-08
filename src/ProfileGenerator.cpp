#include "ProfileGenerator.h"
#include "ConfigManager.h"
#define CPPHTTPLIB_OPENSSL_SUPPORT 
#include "httplib.h"
#include "json.hpp"
#include <iostream>
#include <random>
#include <chrono>
#include <vector>
#include <string>

using json = nlohmann::json;

static std::string base64_decode(const std::string &in) {
    std::string out;
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++) T["ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[i]] = i;
    int val = 0, valb = -8;
    for (unsigned char c : in) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

static std::string getEntropySeed() {
    static std::mt19937 gen(std::chrono::system_clock::now().time_since_epoch().count());
    std::uniform_int_distribution<> dis(1, 9999999);
    return std::to_string(dis(gen));
}

static std::string callLLMAPI(const std::string& prompt, bool jsonMode = false) {
    httplib::Client cli("https://api.deepseek.com");
    cli.set_read_timeout(60, 0);

    json reqBody = {
        {"model", "deepseek-v4-pro"},
        {"temperature", 0.85},
        {"messages", json::array({{{"role", "user"}, {"content", prompt}}})}
    };
    
    // 强制 JSON 输出模式（如果是支持的 API）
    if (jsonMode) {
        reqBody["response_format"] = {{"type", "json_object"}};
    }

    std::string apiKey = ConfigManager::getInstance().getApiKey();
    httplib::Headers headers = {
        {"Authorization", "Bearer " + apiKey},
        {"Content-Type", "application/json"}
    };

    auto res = cli.Post("/v1/chat/completions", headers, reqBody.dump(), "application/json");
    if (res && res->status == 200) {
        json resJson = json::parse(res->body);
        return resJson["choices"][0]["message"]["content"].get<std::string>();
    }
    return "";
}

//角色档案生成
std::future<CharacterProfile> ProfileGenerator::generateRandomProfileAsync(const std::string& worldSetting, const Player& player) {
    return std::async(std::launch::async, [worldSetting, player]() {
        CharacterProfile profile;
        // 【核心锚点】：如果玩家不填，默认就是普通高中
        std::string actualWorld = worldSetting.empty() ? "现代日常都市，普通的高中校园" : worldSetting;

        std::vector<std::string> styles = {"普通的黑发少女"};
        std::vector<std::string> archetypes = {"温柔内向"};
        std::vector<std::string> secrets = {"没有什么特别的烦恼"};

        // 动态加载外部配置文件 traits.json
        std::ifstream file("traits.json");
        if (file.is_open()) {
            try {
                json j;
                file >> j;
                if (j.contains("npc_appearances") && !j["npc_appearances"].empty()) 
                    styles = j["npc_appearances"].get<std::vector<std::string>>();
                if (j.contains("npc_archetypes") && !j["npc_archetypes"].empty()) 
                    archetypes = j["npc_archetypes"].get<std::vector<std::string>>();
                if (j.contains("npc_secrets") && !j["npc_secrets"].empty()) 
                    secrets = j["npc_secrets"].get<std::vector<std::string>>();
            } catch (const std::exception& e) {
                std::cerr << "[Warning] 解析 traits.json 失败, 使用默认设定: " << e.what() << std::endl;
            }
            file.close();
        } else {
            std::cerr << "[Warning] 未找到 traits.json 配置文件，使用默认设定。" << std::endl;
        }

        // 随机抽取
        std::mt19937 gen(std::chrono::system_clock::now().time_since_epoch().count());
        std::string randomStyle = styles[std::uniform_int_distribution<>(0, styles.size() - 1)(gen)];
        std::string randomArchetype = archetypes[std::uniform_int_distribution<>(0, archetypes.size() - 1)(gen)];
        std::string randomSecret = secrets[std::uniform_int_distribution<>(0, secrets.size() - 1)(gen)];

        std::string playerInfo = 
            "【面对的玩家（主角）设定】\n"
            "名字：" + player.getName() + "\n"
            "身世背景：" + player.getBackstory() + "\n"
            "当前六维属性：体格=" + std::to_string(player.getPhysique()) + 
            "，智识=" + std::to_string(player.getIntellect()) + 
            "，魅力=" + std::to_string(player.getCharm()) + 
            "，财力=" + std::to_string(player.getWealth()) + 
            "，共情=" + std::to_string(player.getEmpathy()) + 
            "，运气=" + std::to_string(player.getLuck()) + "\n";

        std::string prompt = 
            "【角色设定】\n"
            "你是一位顶级的 Galgame（恋爱模拟游戏）剧本家，擅长塑造细腻、真实的女性角色。\n\n"
            "【当前世界观设定】\n" + actualWorld + "\n\n"
            "【极度强制的设定组合】（请务必将以下三种元素完美融合到角色中，产生反差感）：\n"
            "1. 外貌风格锚点：【" + randomStyle + "】\n"
            "2. 核心性格原型：【" + randomArchetype + "】\n"
            "3. 隐藏的秘密/创伤：【" + randomSecret + "】\n\n"
            "【核心生成规则】\n"
            "1. 如果【当前世界观设定】为“现代日常都市”、“普通高中”或为空，你必须严格遵循以下限制：\n"
            "   - 绝对禁止：任何魔法、科幻、超能力、穿越、异世界、霸道总裁等夸张元素。\n"
            "2. 追求真实感与合理性：不需要刻意制造夸张的反差。请生动、合理地解释她的经历是如何塑造她的性格与外表的，使她成为一个有血有肉的真实角色。\n\n"
            "2. 如果【当前世界观设定】明确指定了其他背景，请忽略第1条，并严格贴合指定的世界观生成角色。\n\n"
            "3. 【外貌生成建议】：请重点参考这种风格进行描写——【" + randomStyle + "】。请发挥想象力，保证发型、发色等风格的多样性。拒绝幼态。\n\n"
            "4. 【初始好感度动态判定（极度重要）】：请综合考量该角色的【核心性格】与【玩家主角的属性/身世】和【对主角的初始态度】来推算 initial_affection。范围在 0 到 +30 之间。\n"
            "   - 例如：如果玩家魅力极高，且该角色是颜控，初始好感度可以较高；\n"
            "   - 如果玩家财力极高，且该角色背负债务，初始好感度可能也会偏高（甚至带有功利性）；\n"
            "   - 请绝对不要再机械地输出 0！要有逻辑地进行数值赋予！\n\n"
            "【输出要求】\n"
            "【极度重要】：JSON 的键值对之间必须严格使用英文字符的半角逗号（,）分隔！绝对禁止使用中文逗号（，）！\n"
            "必须且只能输出合法的 JSON 格式，字段如下：\n"
            "{\n"
            "  \"name\": \"角色的全名\",\n"
            "  \"appearance\": \"外貌描写（发型、发色、瞳色、服装细节等等，足够详细具体）\",\n"
            "  \"personality_core\": \"核心性格特点，足够详细具体\",\n"
            "  \"hidden_trauma\": \"隐藏的烦恼或执念，足够详细具体\",\n"
            "  \"initial_attitude\": \"对主角的初始态度\",\n"
            "  \"initial_affection\": 20\n"
            "}";

        std::string response = callLLMAPI(prompt, true);
        
        try {
            json j = json::parse(response);
            profile.name = j.value("name", "神秘少女");
            profile.appearance = j.value("appearance", "普通的黑发少女");
            profile.personality_core = j.value("personality_core", "温柔内向");
            profile.hidden_trauma = j.value("hidden_trauma", "没有烦恼");
            profile.initial_attitude = j.value("initial_attitude", "好奇");
            profile.initial_affection = j.value("initial_affection", 0);
            profile.is_generated = true;
        } catch (...) {
            std::cerr << "JSON 解析失败: " << response << std::endl;
        }
        return profile;
    });
}

// 生成玩家档案的实现
std::future<Player> ProfileGenerator::generatePlayerProfileAsync(const std::string& worldSetting) {
    return std::async(std::launch::async, [worldSetting]() {
        std::string actualWorld = worldSetting.empty() ? "现代日常都市，普通的高中校园" : worldSetting;
        
        std::vector<std::string> playerBackgrounds = {"一个随处可见的普通学生。"};

        // 动态加载外部配置文件
        std::ifstream file("traits.json");
        if (file.is_open()) {
            try {
                json j;
                file >> j;
                if (j.contains("player_backgrounds") && !j["player_backgrounds"].empty()) {
                    playerBackgrounds = j["player_backgrounds"].get<std::vector<std::string>>();
                }
            } catch (...) {}
            file.close();
        }

        std::mt19937 gen(std::chrono::system_clock::now().time_since_epoch().count());
        std::string randomBg = playerBackgrounds[std::uniform_int_distribution<>(0, playerBackgrounds.size() - 1)(gen)];
        
        std::string prompt = 
            "【设定说明】\n"
            "作为Galgame策划，请根据当前世界观：【" + actualWorld + "】，生成玩家的主角设定。\n\n"
            "【强制的主角开局处境】\n"
            "请基于以下处境进行深度扩写和润色：【" + randomBg + "】\n\n"
            "【核心约束】\n"
            "1. 如果世界观是“现代/普通高中”或为空，主角必须是一个男高中生。背景应当贴近日常（例如：刚搬家过来的转校生、青梅竹马的邻居等）。\n"
            "2. 绝对禁止“龙傲天”、“兵王回归”、“隐藏富二代”等夸张爽文设定。\n"
            "3. 如果是其他明确的世界观（如赛博朋克），请符合该世界观的底层逻辑，设定一个有代入感的角色。\n\n"
            "4. 【核心任务】：请根据你为主角编写的背景故事，合理分配他的 TRPG 六维属性（0-100）。\n"
            "   - 体格(physique)、智识(intellect)、魅力(charm)、财力(wealth)、共情(empathy)、运气(luck)。\n"
            "   - 属性总和请严格控制在 360 - 480 之间，以体现角色的长处与短板，一定要符合故事逻辑！\n\n"
            "【输出格式】\n"
            "必须且只能返回JSON：\n"
            "【极度重要】：JSON 的键值对之间必须严格使用英文字符的半角逗号（,）分隔！绝对禁止使用中文逗号（，）！\n"
            "{\n"
            "  \"name\": \"主角名字\",\n"
            "  \"backstory\": \"详细阐述主角的背景、身世、性格底色以及目前的处境，要求详细阐述，使玩家有足够的了解，有代入感。\",\n"
            "  \"physique\": 50,\n"
            "  \"intellect\": 60,\n"
            "  \"charm\": 55,\n"
            "  \"wealth\": 40,\n"
            "  \"empathy\": 70,\n"
            "  \"luck\": 60\n"
            "}";

        std::string response = callLLMAPI(prompt, true);
        Player newPlayer("主角");
        
        try {
            json j = json::parse(response);
            newPlayer = Player(j.value("name", "主角"));
            newPlayer.setBackstory(j.value("backstory", "一个随处可见的普通学生。"));
            
            // 提取 AI 智能分配的六维属性
            newPlayer.setAttributes(
                j.value("physique", 50),
                j.value("intellect", 50),
                j.value("charm", 50),
                j.value("wealth", 50),
                j.value("empathy", 50),
                j.value("luck", 50)
            );
        } catch (const std::exception& e) {
            // 【新增】：打印详细报错，方便排查
            std::cerr << "[Error] 玩家数据解析失败: " << e.what() << std::endl;
            std::cerr << "[Error] 大模型原始返回内容: {" << response << "}" << std::endl;
            newPlayer.setBackstory("普通的转校生。（量子发生扰动，记忆解析失败）");
        }
        return newPlayer;
    });
}

std::future<std::string> ProfileGenerator::generateRandomWorldSettingAsync() {
    return std::async(std::launch::async, []() {
        std::string prompt = 
            "你是一位顶级的世界观架构师和Galgame文案策划。请随机生成一个极具沉浸感和代入感的【游戏世界观设定】。\n\n"
            "【生成随机熵】： " + getEntropySeed() + "（请利用此随机数确保本次生成的世界与以往不同，最大化多样性）\n\n"
            "【概率分布要求】\n"
            "1. 【70%的概率】为现代日常/青春校园（如：海滨小镇的公立高中、注重升学率的严格私立高中、充满艺术气息的美术附中、偏远乡村的唯一中学等等）。\n"
            "2. 【30%的概率】为特殊设定（如：赛博朋克都市的底层街区、剑与魔法的和平小镇、末日后的一座废土庇护所等等）。\n\n"
            "【生成内容维度（必须包含）】\n"
            "1. 时代与背景：这是一个怎样的时代？\n"
            "2. 环境与氛围：请加入具体的视觉、听觉或嗅觉描写。\n"
            "3. 核心冲突/日常基调：这个世界的人们在为了什么而活？\n\n"
            "【输出格式】\n"
            "不要任何废话、不要分点作答。请直接用一段优美、充满沉浸感的散文式描述输出，要求详细阐述，使玩家有足够的了解，有代入感。\n";

        // 注意这里调用时不强制 JSON 模式，因为只要一句话
        std::string response = callLLMAPI(prompt, false);
        
        // 去除模型可能返回的多余换行符
        response.erase(std::remove(response.begin(), response.end(), '\n'), response.end());
        response.erase(std::remove(response.begin(), response.end(), '\r'), response.end());
        
        if (response.empty()) {
            return std::string("普通的现代公立高中"); // 兜底
        }
        return response;
    });
}

std::future<GameEvent> ProfileGenerator::generateRandomEventAsync(const std::string& worldSetting, const Player& player, const CharacterProfile& npc, const std::string& chatContext) {
    return std::async(std::launch::async, [worldSetting, player, npc, chatContext]() {
        GameEvent event;
        
        // 1. 世界观兜底：空白则默认为普通高中
        std::string actualWorld = worldSetting.empty() ? "现代日常都市，普通的高中校园" : worldSetting;

        // 2. 注入强力约束的 Prompt 【核心修改点：要求 AI 输出每个选项对应的检定属性】
        std::string prompt = 
            "你是一个极其优秀的 TRPG Game Master (游戏主持)。\n"
            "【当前世界观】：" + actualWorld + "\n"
            "【玩家设定】：" + player.getName() + "，身世：" + player.getBackstory() + "\n"
            "【当前互动的 NPC】：" + npc.name + "，性格：" + npc.personality_core + "，隐藏执念：" + npc.hidden_trauma + "\n"
            "【最近的对话上下文】\n" + chatContext + "\n\n"
            "【核心约束规则】\n"
            "1. 如果【当前世界观】是“现代日常”、“普通高中”或类似设定，绝对禁止出现魔法、科幻、超能力、凶杀、黑帮等脱离日常的夸张要素！事件必须是普通的日常风波。\n"
            "2. 如果世界观是其他特定背景，请严格贴合该设定的底层逻辑来生成事件。\n\n"
            "NPC 刚刚发出了推进剧情的信号。请你务必【无缝顺承当前的对话情境】，生成一个推动故事发展的【剧情事件】。\n"
            "请提供 3 个符合当前情境的不同行动选项，并为每个选项指定一个最相关的检测属性（只能是以下英文单词之一: physique, intellect, charm, wealth, empathy, luck）。\n"
            "必须严格输出纯 JSON 格式：\n"
            "{\n"
            "  \"description\": \"事件或场景的生动描写\",\n"
            "  \"choices\": [\n"
            "    {\"text\": \"强行突破阻碍\", \"stat\": \"physique\"},\n"
            "    {\"text\": \"尝试温柔说服\", \"stat\": \"charm\"},\n"
            "    {\"text\": \"静观其变\", \"stat\": \"luck\"}\n"
            "  ]\n"
            "}";

        // 3. 调用重构后的网络通信接口（强制 JSON 模式）
        std::string response = callLLMAPI(prompt, true);

        // 4. 解析与安全防崩溃兜底 【核心修改点：按 EventChoice 结构体解析】
        try {
            json eventJson = json::parse(response);
            event.description = eventJson.value("description", "一阵微风吹过，气氛变得有些微妙。");
            
            if (eventJson.contains("choices") && eventJson["choices"].is_array()) {
                for (const auto& choice : eventJson["choices"]) {
                    // 把 JSON 对象转化为 EventChoice 结构体推入数组
                    event.choices.push_back({
                        choice.value("text", "静观其变"), 
                        choice.value("stat", "luck") // 如果 AI 忘记写属性，兜底用 luck
                    });
                }
            }
            
            // 极端兜底：如果 AI 没生成选项，强制塞入三个带属性的通用选项
            if (event.choices.empty()) {
                event.choices = {
                    {"静观其变", "luck"}, 
                    {"尝试交流", "empathy"}, 
                    {"微微一笑", "charm"}
                };
            }
            event.is_valid = true;

        } catch (...) {
            std::cerr << "解析事件 JSON 失败: " << response << std::endl;
            // 发生异常时，给玩家一个安全的台阶下
            event.description = "（系统旁白：命运的齿轮似乎短暂地卡住了，但生活还在继续。）";
            event.choices = {
                {"尝试继续聊天", "charm"}, 
                {"保持沉默", "intellect"},
                {"深呼吸", "physique"}
            };
            event.is_valid = true; 
        }

        return event;
    });
}
// 场景导入生成逻辑
std::future<std::string> ProfileGenerator::generateEncounterAsync(const std::string& worldSetting, const Player& player, const CharacterProfile& npcProfile) {
    return std::async(std::launch::async, [worldSetting, player, npcProfile]() {
        std::string actualWorld = worldSetting.empty() ? "现代日常都市，普通的高中校园" : worldSetting;

        std::string prompt = 
            "【角色设定】\n你是一位 Galgame 剧本家，请为游戏撰写一段男女主角的【初遇场景】（导入幕）。\n\n"
            "【背景信息】\n"
            "当前世界观：" + actualWorld + "\n"
            "玩家信息：" + player.getName() + "，身世：" + player.getBackstory() + "\n"
            "女主信息：" + npcProfile.name + "，外貌：" + npcProfile.appearance + "，性格：" + npcProfile.personality_core + "，隐藏执念：" + npcProfile.hidden_trauma + "\n\n"
            "【剧情要求】\n"
            "1. 请描写他们在什么具体地点、因为什么具体事件相遇。\n"
            "2. 侧重于描写女主的神态、动作以及周围的环境氛围。\n"
            "3. 如果世界观是普通现代/高中，请将其限制在校园、放学路、书店、便利店等日常场景。\n"
            "4. 必须以旁白（第二人称“你”）的口吻叙述。\n"
            "5. 字数控制在300字左右，结尾自然引出女主的反应，把聊天的机会留给玩家。\n\n"
            "请直接输出优美的旁白文本：";

        return callLLMAPI(prompt, false);
    });
}

std::future<std::pair<std::string, bool>> ProfileGenerator::generateTransitionSceneAsync(const std::string& worldSetting, const std::string& recentContext, const std::string& currentTimeStr) {
    return std::async(std::launch::async, [worldSetting, recentContext, currentTimeStr]() {
        std::string actualWorld = worldSetting.empty() ? "现代日常都市" : worldSetting;
        
        std::string prompt = 
            "作为Galgame剧本家，请根据以下对话上下文，生成一段【场景转移与新场景开场】的旁白（第二人称）。\n\n"
            "【当前世界观】： " + actualWorld + "\n"
            "【当前宏观时间】： " + currentTimeStr + "\n"
            "【最近的对话】\n" + recentContext + "\n\n"
            "【剧情判断与输出规则】\n"
            "1. 走向判断：如果两人只是换个附近的地点（如从街头走到咖啡厅、从客厅走到阳台），将 `is_time_advanced` 设为 false。如果两人明确告别、暗示经过了漫长时间或到了第二天，将 `is_time_advanced` 设为 true。\n"
            "2. 旁白内容约束（极度重要）：绝对不要把旁白写成“大结局”或“就此分别”的收尾！\n"
            "   - 首先，用一两句话简要描写移动的过程或时光的流逝。\n"
            "   - 接着，重点描写你们已经到达了新的具体地点。（例如：咖啡馆的座位上、新一天的教室里、深夜的岔路口停下脚步）。\n"
            "   - 旁白的最后一句，必须定格在【两人面对面，准备开启新一轮互动】的瞬间，彻底拉开新场景的帷幕，为接下来角色的开口留足空间。\n"
            "3. 不要包含人物具体的对话，只侧重环境与氛围或场景的转换。极具画面感，自然衔接上下文。\n"
            "4. 必须且只能输出合法的 JSON 格式：\n"
            "{\n"
            "  \"description\": \"旁白内容\",\n"
            "  \"is_time_advanced\": false\n"
            "}";

        std::string response = callLLMAPI(prompt, true); // 强制开启 JSON 模式

        try {
            json j = json::parse(response);
            std::string desc = j.value("description", "（周围的场景和氛围悄然发生了一些变化...）");
            bool advanced = j.value("is_time_advanced", false);
            return std::make_pair(desc, advanced);
        } catch (...) {
            std::cerr << "转场JSON解析失败: " << response << std::endl;
            return std::make_pair(std::string("（时空发生了一丝涟漪...）"), false); 
        }
    });
}

std::string ProfileGenerator::generateBackgroundAsync(const std::string& sceneDescription) {
    std::cout << "[System] 开始根据场景重构世界画面..." << std::endl;
    try {
        // ==========================================
        // 1. 准备 API Key
        // ==========================================
        std::string apiKey = ConfigManager::getInstance().getImageApiKey();
        // 清理可能由于读取配置带来的多余换行符
        apiKey.erase(std::remove(apiKey.begin(), apiKey.end(), '\n'), apiKey.end());
        apiKey.erase(std::remove(apiKey.begin(), apiKey.end(), '\r'), apiKey.end());
        apiKey.erase(apiKey.find_last_not_of(" ") + 1);

        if (apiKey.empty()) {
            std::cerr << "[Error] 生图 API Key 为空，请检查 config.json" << std::endl;
            return "";
        }

        // ==========================================
        // 2. 请求生图 API 
        // ==========================================
        // 构造背景的专属提示词
        std::string prompt = 
            "A masterpiece 2D anime background CG for a Japanese visual novel. "
            "【Art Style】: Authentic Japanese Anime Key Visual, Makoto Shinkai style but strictly 2D painted. High-quality 2D anime background art (动画场景美术), beautiful anime scenery, gorgeous sky and environment. "
            "【STRICT EXCLUSIONS】: ABSOLUTELY NO humans, NO characters, NO animals, NO living creatures! The scene MUST be completely empty of people. NO 3D rendering, NO Unreal Engine, NO photography, NO realistic textures. Keep it pure 2D illustration. "
            "请根据以下剧情场景的描写进行完美的构图：【" + sceneDescription + "】。";

        json requestBody = {
            {"model", "openai/gpt-5.4-image-2"}, 
            {"messages", json::array({{
                {"role", "user"},
                {"content", prompt}
            }})},
            {"modalities", json::array({"image"})} 
        };

        httplib::Client cli("https://openrouter.ai");
        std::string proxyHost = ConfigManager::getInstance().getProxyHost();
        int proxyPort = ConfigManager::getInstance().getProxyPort();
        if (!proxyHost.empty() && proxyPort > 0) {
            cli.set_proxy(proxyHost, proxyPort);
        }
        cli.enable_server_certificate_verification(false);
        cli.set_read_timeout(360, 0);// 生图比较慢，给足 120 秒等待

        httplib::Headers headers = { 
            {"Authorization", "Bearer " + apiKey},
            {"Content-Type", "application/json"}
        };
        
        auto res = cli.Post("/api/v1/chat/completions", headers, requestBody.dump(), "application/json");

        if (!res || res->status != 200) {
            if (res) std::cerr << "OpenRouter生图失败: 状态码 " << res->status << " - " << res->body << std::endl;
            else std::cerr << "OpenRouter网络连接失败/超时" << std::endl;
        }

        json resJson = json::parse(res->body);
            std::string b64dataUrl = resJson["choices"][0]["message"]["images"][0]["image_url"]["url"];

            size_t commaPos = b64dataUrl.find(',');
            if (commaPos == std::string::npos) return "";
            std::string actualBase64 = b64dataUrl.substr(commaPos + 1);

            std::string binaryImageData = base64_decode(actualBase64);

            auto t = std::time(nullptr);
            struct tm tm_info;
    #ifdef _WIN32
            localtime_s(&tm_info, &t); 
    #else
            localtime_r(&t, &tm_info); 
    #endif
            char timeBuf[128];
            std::strftime(timeBuf, sizeof(timeBuf), "%Y%m%d_%H%M%S", &tm_info);
            
            std::string savePath = "saves/bg_" + std::string(timeBuf) + ".png";
            
            std::ofstream file(savePath, std::ios::binary);
            if (file.is_open()) {
                file.write(binaryImageData.data(), binaryImageData.size());
                file.close();
                std::cout << "[System] 场景重构完毕，已保存至: " << savePath << std::endl;
                return savePath; 
            }
    } catch (const std::exception& e) {
        std::cerr << "[Error] 解析或保存背景图异常: " << e.what() << std::endl;
    }
    return "";
}