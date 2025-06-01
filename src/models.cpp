
#include "models.h"

#include "args.h"
#include "openai.h"

int models() {
  try {
    OpenAIClient client;
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
