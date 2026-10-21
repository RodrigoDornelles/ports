-- No CJK fonts
--
-- Removes the Chinese, Japanese and Korean fonts (62 MB), loaded even in
-- English; those languages fall back to the main font.

--! @delete resources/fonts/GoNotoCJKCore.ttf
--! @delete resources/fonts/GoNotoCurrent-Bold.ttf
--! @delete resources/fonts/NotoSansJP-Bold.ttf
--! @delete resources/fonts/NotoSansKR-Bold.ttf
--! @delete resources/fonts/NotoSansSC-Bold.ttf
--! @delete resources/fonts/NotoSansTC-Bold.ttf

--! @patch game.lua 950-951
            ['ja'] = {font = 1, label = "日本語", key = 'ja', beta = nil, button = "提案する", warning = {'この翻訳は現在ベータ版です。提案があった場合、','ボタンをクリックしてください。', "もう一度クリックして確認"}},
            ['ko'] = {font = 1, label = "한국어", key = 'ko', beta = nil, button = "번역 피드백", warning = {'이 언어는 아직 베타 단계에 있습니다. ','번역을 도와주시려면 피드백 버튼을 눌러주세요.', "다시 클릭해서 확인하세요"}},
--! @endpatch

--! @patch game.lua 956-957
            ['zh_CN'] = {font = 1, label = "简体中文", key = 'zh_CN', beta = nil, button = "意见反馈", warning = {'这个语言目前尚为Beta版本。 请帮助我们改善翻译品质，','点击”意见反馈” 来提供你的意见。', "再次点击确认"}},
            ['zh_TW'] = {font = 1, label = "繁體中文", key = 'zh_TW', beta = nil, button = "意見回饋", warning = {'這個語言目前尚為Beta版本。請幫助我們改善翻譯品質，','點擊”意見回饋” 來提供你的意見。', "再按一下即可確認"}},
--! @endpatch
