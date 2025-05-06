
#include "models.h"

#include "./args.h"
#include "./openai.h"

int models(AiArgs const& args) {
    try {
        OpenAIClient client(args);
        auto models = client.models();
        for (auto& model : models) {
            std::cout << model << '\n';
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
