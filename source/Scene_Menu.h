#include "Scene.h"

class Scene_Menu : public Scene {
protected:
  std::string m_title;
  std::vector<std::string> m_menuStrings;
  std::vector<std::string> m_levelPaths;
  sf::Text m_menuText;
  int m_selectedMenuIndex = 0;

  void init();
  void onEnd();

public:
  Scene_Menu();
  Scene_Menu(GameEngine *gameEngine = nullptr);

  void update();
  void sDoAction(const Action &action);
  void sRender();
};
