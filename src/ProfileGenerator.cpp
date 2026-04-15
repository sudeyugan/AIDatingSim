#include "ProfileGenerator.h"
#include "ConfigManager.h"
#define CPPHTTPLIB_OPENSSL_SUPPORT 
#include "httplib.h"
#include "json.hpp"
#include <iostream>
#include <random>
#include <chrono>

using json = nlohmann::json;

static std::string getEntropySeed() {
    static std::mt19937 gen(std::chrono::system_clock::now().time_since_epoch().count());
    std::uniform_int_distribution<> dis(1, 9999999);
    return std::to_string(dis(gen));
}

static std::string callLLMAPI(const std::string& prompt, bool jsonMode = false) {
    httplib::Client cli("https://api.deepseek.com");
    cli.set_read_timeout(30, 0);

    json reqBody = {
        {"model", "deepseek-chat"},
        {"messages", json::array({{{"role", "user"}, {"content", prompt}}})},
        {"temperature", 0.7}
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
std::future<CharacterProfile> ProfileGenerator::generateRandomProfileAsync(const std::string& worldSetting) {
    return std::async(std::launch::async, [worldSetting]() {
        CharacterProfile profile;
        // 【核心锚点】：如果玩家不填，默认就是普通高中
        std::string actualWorld = worldSetting.empty() ? "现代日常都市，普通的高中校园" : worldSetting;

        std::string prompt = 
            "【角色设定】\n"
            "你是一位顶级的 Galgame（恋爱模拟游戏）剧本家，擅长塑造细腻、真实、有反差感的女性角色。\n\n"
            "【当前世界观设定】\n" + actualWorld + "\n\n"
            "【核心生成规则】\n"
            "1. 如果【当前世界观设定】为“现代日常都市”、“普通高中”或为空，你必须严格遵循以下限制：\n"
            "   - 绝对禁止：任何魔法、科幻、超能力、穿越、异世界、霸道总裁、黑帮大小姐等夸张元素。\n"
            "   - 角色身份限制：她必须是一个的高中生（例如：班委、社团成员、隔壁班同学、图书管理员等）。\n"
            "   - 风格：日常、青春、治愈或带着青春期的烦恼（如升学压力、人际关系、暗恋等）,也有一定概率包含不为人知的心理创伤。\n"
            "2. 如果【当前世界观设定】明确指定了其他背景，请忽略第1条，并严格贴合指定的世界观生成角色。\n\n"
            "【输出要求】\n"
            "必须且只能输出合法的 JSON 格式，字段如下：\n"
            "{\n"
            "  \"name\": \"角色的全名\",\n"
            "  \"appearance\": \"外貌描写（发型、发色、瞳色、服装细节，用于AI绘画）\",\n"
            "  \"personality_core\": \"核心性格特点\",\n"
            "  \"hidden_trauma\": \"隐藏的烦恼或执念\",\n"
            "  \"initial_attitude\": \"对主角的初始态度\",\n"
            "  \"initial_affection\": 0\n"
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
std::future<std::pair<std::string, std::string>> ProfileGenerator::generatePlayerProfileAsync(const std::string& worldSetting) {
    return std::async(std::launch::async, [worldSetting]() {
        std::string actualWorld = worldSetting.empty() ? "现代日常都市，普通的高中校园" : worldSetting;
        
        std::string prompt = 
            "【设定说明】\n"
            "作为Galgame策划，请根据当前世界观：【" + actualWorld + "】，生成玩家的主角设定。\n\n"
            "【核心约束】\n"
            "1. 如果世界观是“现代/普通高中”或为空，主角必须是一个男高中生。背景应当贴近日常（例如：刚搬家过来的转校生、青梅竹马的邻居等）。\n"
            "2. 绝对禁止“龙傲天”、“兵王回归”、“隐藏富二代”等夸张爽文设定。\n"
            "3. 如果是其他明确的世界观（如赛博朋克），请符合该世界观的底层逻辑，设定一个有代入感的角色。\n\n"
            "【输出格式】\n"
            "必须且只能返回JSON：\n"
            "{\n"
            "  \"name\": \"主角名字\",\n"
            "  \"backstory\": \"详细阐述主角的背景和身世\"\n"
            "}";

        std::string response = callLLMAPI(prompt, true);
        try {
            json j = json::parse(response);
            return std::make_pair(j.value("name", "主角"), j.value("backstory", "一个随处可见的普通学生。"));
        } catch (...) {
            return std::make_pair(std::string("主角"), std::string("普通的转校生。"));
        }
    });
}

std::future<std::string> ProfileGenerator::generateRandomWorldSettingAsync() {
    return std::async(std::launch::async, []() {
        std::string prompt = 
            "你是一位顶级的世界观架构师和Galgame文案策划。请随机生成一个极具沉浸感和代入感的【游戏世界观设定】。\n\n"
            "【概率分布要求】\n"
            "1. 【70%的概率】为现代日常/青春校园（如：海滨小镇的公立高中、注重升学率的严格私立高中、充满艺术气息的美术附中、偏远乡村的唯一中学等等）。\n"
            "2. 【30%的概率】为特殊设定（如：赛博朋克都市的底层街区、剑与魔法的和平小镇、末日后的一座废土庇护所等等）。\n\n"
            "【生成内容维度（必须包含）】\n"
            "1. 时代与背景：这是一个怎样的时代？\n"
            "2. 环境与氛围：请加入具体的视觉、听觉或嗅觉描写。\n"
            "3. 核心冲突/日常基调：这个世界的人们在为了什么而活？\n\n"
            "【输出格式】\n"
            "不要任何废话、不要分点作答。请直接用一段优美、充满沉浸感的散文式描述输出。\n";

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

        // 2. 注入强力约束的 Prompt
        std::string prompt = 
            "你是一个极其优秀的 TRPG Game Master (游戏主持)。\n"
            "【当前世界观】：" + actualWorld + "\n"
            "【玩家设定】：" + player.getName() + "，身世：" + player.getBackstory() + "\n"
            "【当前互动的 NPC】：" + npc.name + "，性格：" + npc.personality_core + "，隐藏执念：" + npc.hidden_trauma + "\n"
            "【最近的对话上下文】\n" + chatContext + "\n\n"
            "【核心约束规则】\n"
            "1. 如果【当前世界观】是“现代日常”、“普通高中”或类似设定，绝对禁止出现魔法、科幻、超能力、凶杀、黑帮等脱离日常的夸张要素！事件必须是普通的日常风波（例如：突然下雨没带伞、值日时的意外、尴尬的肢体接触、遇到严厉的老师、氛围暧昧的沉默等）。\n"
            "2. 如果世界观是其他特定背景，请严格贴合该设定的底层逻辑来生成事件。\n\n"
            "NPC 刚刚发出了推进剧情的信号。请你务必【无缝顺承当前的对话情境】，生成一个推动故事发展的【剧情事件】。\n"
            "请提供 3 个符合当前情境的不同行动选项（例如：A.主动询问 B.静观其变 C.温柔解围），尽量让选项体现出不同的性格倾向。\n"
            "必须严格输出纯 JSON 格式：\n"
            "{\n"
            "  \"description\": \"事件或场景的生动描写（注重画面感和氛围，50-100字左右）\",\n"
            "  \"choices\": [\"选项1文字\", \"选项2文字\", \"选项3文字\"]\n"
            "}";

        // 3. 调用重构后的网络通信接口（强制 JSON 模式）
        std::string response = callLLMAPI(prompt, true);

        // 4. 解析与安全防崩溃兜底
        try {
            json eventJson = json::parse(response);
            event.description = eventJson.value("description", "一阵微风吹过，气氛变得有些微妙。");
            
            if (eventJson.contains("choices") && eventJson["choices"].is_array()) {
                for (const auto& choice : eventJson["choices"]) {
                    event.choices.push_back(choice.get<std::string>());
                }
            }
            
            // 极端兜底：如果 AI 没生成选项，强制塞入三个通用选项，防止游戏 UI 没有按钮可点
            if (event.choices.empty()) {
                event.choices = {"静观其变", "转移话题", "微微一笑"};
            }
            event.is_valid = true;

        } catch (...) {
            std::cerr << "解析事件 JSON 失败: " << response << std::endl;
            // 发生异常时，给玩家一个安全的台阶下
            event.description = "（系统旁白：命运的齿轮似乎短暂地卡住了，但生活还在继续。）";
            event.choices = {"尝试继续聊天", "保持沉默", "深呼吸"};
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
            "作为Galgame剧本家，请根据以下对话上下文，生成一段场景转移的旁白（第二人称）。\n\n"
            "【当前世界观】： " + actualWorld + "\n"
            "【当前宏观时间】： " + currentTimeStr + "\n"
            "【最近的对话】\n" + recentContext + "\n\n"
            "【剧情判断与输出规则】\n"
            "1. 请判断剧情走向：如果两人只是换个附近的地点继续相处（如从街头走到咖啡厅、从客厅走到阳台），请将 `is_time_advanced` 设为 false。如果两人明确告别、各自回家、或者暗示经过了漫长的时间，请将 `is_time_advanced` 设为 true。\n"
            "2. 生成的 `description` 不要包含人物具体的对话，只侧重于描写环境的变化、两人行动状态的改变以及时光的流逝。字数约100字，极具画面感，自然衔接上下文。\n"
            "3. 必须且只能输出合法的 JSON 格式：\n"
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