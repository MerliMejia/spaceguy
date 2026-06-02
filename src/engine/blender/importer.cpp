#include "importer.h"

static std::vector<std::string> currentTokens;
static std::size_t cursor = 0;

static std::vector<std::string> readTokensIgnoringComments(std::istream &input)
{
    std::vector<std::string> tokens;
    std::string line;

    while (std::getline(input, line))
    {
        if (line.empty() || line[0] == '#')
        {
            continue;
        }

        std::istringstream lineInput(line);

        std::string token;
        while (lineInput >> token)
        {
            tokens.push_back(token);
        }
    }

    return tokens;
}

static std::string next()
{
    if (cursor >= currentTokens.size())
    {
        throw std::runtime_error("Unexpected end of file");
    }

    return currentTokens[cursor++];
}

static void expect(const std::string &expected)
{
    const std::string actual = next();

    if (actual != expected)
    {
        throw std::runtime_error("Expected '" + expected + "'" + ", got '" + actual + "'");
    }
}

static int readInt()
{
    return std::stoi(next());
}

static std::size_t readSize()
{
    return static_cast<std::size_t>(std::stoul(next()));
}

static std::uint32_t readUInt32()
{
    return static_cast<std::uint32_t>(std::stoul(next()));
}

static float readFloat()
{
    return std::stof(next());
}

static std::string readString()
{
    return next();
}

BlenderModel loadModel(const std::string &path)
{
    std::ifstream file(path);

    if (!file)
    {
        throw std::runtime_error("Could not open file: " + path);
    }

    currentTokens.clear();
    cursor = 0;

    BlenderModel model;

    currentTokens = readTokensIgnoringComments(file);

    expect("spaceguy_3d");

    const int version = readInt();

    if (version != 2)
    {
        throw std::runtime_error("Unsupported .3d version");
    }

    expect("object_name");
    model.name = readString();

    expect("fps");
    model.fps = readFloat();

    expect("vertex_count");
    const std::size_t vertexCount = readSize();

    expect("index_count");
    const std::size_t indexCount = readSize();

    expect("animation_count");
    const std::size_t animationCount = readSize();

    expect("vertices");

    model.vertices.resize(vertexCount);

    for (auto &vertex : model.vertices)
    {
        vertex.pos.x = readFloat();
        vertex.pos.y = readFloat();
        vertex.pos.z = readFloat();

        vertex.color.x = readFloat();
        vertex.color.y = readFloat();
        vertex.color.z = readFloat();
    }

    expect("indices");

    model.indices.resize(indexCount);

    for (auto &index : model.indices)
    {
        index = readUInt32();
    }

    expect("animations");

    model.animations.reserve(animationCount);

    for (std::size_t animationIndex = 0; animationIndex < animationCount; ++animationIndex)
    {
        AnimationClip clip;

        expect("animation");
        clip.name = readString();

        expect("start_frame");
        clip.startFrame = readInt();

        expect("end_frame");
        clip.endFrame = readInt();

        expect("key_pose_count");
        const std::size_t frameCount = readSize();

        clip.keyPoses.resize(frameCount);

        for (auto &keyPose : clip.keyPoses)
        {
            expect("key_pose");
            keyPose.blenderFrame = readInt();

            keyPose.positions.resize(vertexCount);

            for (auto &pos : keyPose.positions)
            {
                pos.x = readFloat();
                pos.y = readFloat();
                pos.z = readFloat();
            }
        }

        model.animations.push_back(std::move(clip));
    }

    return model;
}