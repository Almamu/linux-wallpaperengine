"""Small runtime message catalog for the desktop app and its service.

Portuguese strings are the message IDs, which keeps call sites readable. The
catalog is deliberately self-contained so packaging needs no gettext setup.
"""

from __future__ import annotations

import configparser
import locale
import os
from pathlib import Path


_LANGUAGES: tuple[tuple[str, str], ...] = (
    ("auto", "Automático / System"),
    ("pt_BR", "Português (Brasil)"),
    ("en", "English"),
    ("de", "Deutsch"),
    ("ru", "Русский"),
    ("ja", "日本語"),
    ("zh_CN", "简体中文"),
    ("es", "Español"),
    ("hi", "हिन्दी"),
)

_CODES = tuple(code for code, _name in _LANGUAGES if code != "auto")

# Each row is Portuguese, English, German, Russian, Japanese, Simplified
# Chinese, Spanish, and Hindi, in that order. Keep named format placeholders
# identical in every translation.
_ROWS: tuple[tuple[str, ...], ...] = (
    ("Seu espaço em movimento.", "Your space in motion.", "Dein Raum in Bewegung.", "Ваше пространство в движении.", "動きのあるデスクトップ。", "让桌面动起来。", "Tu espacio en movimiento.", "आपकी चलती-फिरती डेस्कटॉप दुनिया।"),
    ("ESPAÇO DE TRABALHO", "WORKSPACE", "ARBEITSBEREICH", "РАБОЧАЯ ОБЛАСТЬ", "ワークスペース", "工作区", "ESPACIO DE TRABAJO", "कार्यस्थान"),
    ("Estado do motor", "Engine status", "Status der Engine", "Состояние движка", "エンジンの状態", "引擎状态", "Estado del motor", "इंजन की स्थिति"),
    ("Biblioteca", "Library", "Bibliothek", "Библиотека", "ライブラリ", "壁纸库", "Biblioteca", "लाइब्रेरी"),
    ("Playlists", "Playlists", "Wiedergabelisten", "Плейлисты", "プレイリスト", "播放列表", "Listas de reproducción", "प्लेलिस्ट"),
    ("Idiomas", "Languages", "Sprachen", "Языки", "言語", "语言", "Idiomas", "भाषाएँ"),
    ("Configurações", "Settings", "Einstellungen", "Настройки", "設定", "设置", "Configuración", "सेटिंग्स"),
    ("Minimizar", "Minimize", "Minimieren", "Свернуть", "最小化", "最小化", "Minimizar", "न्यूनतम करें"),
    ("Maximizar", "Maximize", "Maximieren", "Развернуть", "最大化", "最大化", "Maximizar", "अधिकतम करें"),
    ("Restaurar", "Restore", "Wiederherstellen", "Восстановить", "元に戻す", "还原", "Restaurar", "पुनर्स्थापित करें"),
    ("Fechar", "Close", "Schließen", "Закрыть", "閉じる", "关闭", "Cerrar", "बंद करें"),
    ("BIBLIOTECA STEAM", "STEAM LIBRARY", "STEAM-BIBLIOTHEK", "БИБЛИОТЕКА STEAM", "STEAM ライブラリ", "STEAM 壁纸库", "BIBLIOTECA DE STEAM", "स्टीम लाइब्रेरी"),
    ("Assine novos wallpapers no Wallpaper Engine original.", "Subscribe to new wallpapers in the original Wallpaper Engine.", "Abonniere neue Wallpaper im originalen Wallpaper Engine.", "Подписывайтесь на новые обои в оригинальном Wallpaper Engine.", "新しい壁紙は元の Wallpaper Engine でサブスクライブしてください。", "请在原版 Wallpaper Engine 中订阅新壁纸。", "Suscríbete a nuevos fondos en el Wallpaper Engine original.", "नए वॉलपेपर मूल Wallpaper Engine में सब्सक्राइब करें।"),
    ("Lendo biblioteca…", "Loading library…", "Bibliothek wird geladen…", "Загрузка библиотеки…", "ライブラリを読み込み中…", "正在读取壁纸库…", "Cargando biblioteca…", "लाइब्रेरी लोड हो रही है…"),
    ("Seus wallpapers instalados, prontos para usar.", "Your installed wallpapers, ready to use.", "Deine installierten Wallpaper, sofort einsatzbereit.", "Установленные обои готовы к использованию.", "インストール済みの壁紙をすぐに使えます。", "已安装的壁纸，随时可用。", "Tus fondos instalados, listos para usar.", "आपके इंस्टॉल किए गए वॉलपेपर इस्तेमाल के लिए तैयार हैं।"),
    ("Organize a rotação do seu jeito.", "Organize rotation your way.", "Gestalte den Wechsel nach deinen Wünschen.", "Настройте смену обоев по-своему.", "好みに合わせて壁紙の切り替えを整理できます。", "按自己的方式安排壁纸轮换。", "Organiza la rotación a tu manera.", "वॉलपेपर बदलने का क्रम अपने हिसाब से सजाएँ।"),
    ("Escolha o idioma da interface.", "Choose the interface language.", "Wähle die Sprache der Oberfläche.", "Выберите язык интерфейса.", "表示言語を選択します。", "选择界面语言。", "Elige el idioma de la interfaz.", "इंटरफ़ेस की भाषा चुनें।"),
    ("Ajuste a reprodução e o aplicativo.", "Adjust playback and the app.", "Passe Wiedergabe und App an.", "Настройте воспроизведение и приложение.", "再生とアプリを設定します。", "调整播放和应用设置。", "Ajusta la reproducción y la aplicación.", "प्लेबैक और ऐप की सेटिंग्स बदलें।"),
    ("Atualizar", "Refresh", "Aktualisieren", "Обновить", "更新", "刷新", "Actualizar", "रीफ़्रेश करें"),
    ("Atualizar biblioteca · Ctrl+R", "Refresh library · Ctrl+R", "Bibliothek aktualisieren · Strg+R", "Обновить библиотеку · Ctrl+R", "ライブラリを更新 · Ctrl+R", "刷新壁纸库 · Ctrl+R", "Actualizar biblioteca · Ctrl+R", "लाइब्रेरी रीफ़्रेश करें · Ctrl+R"),
    ("Atualizar biblioteca", "Refresh library", "Bibliothek aktualisieren", "Обновить библиотеку", "ライブラリを更新", "刷新壁纸库", "Actualizar biblioteca", "लाइब्रेरी रीफ़्रेश करें"),
    ("Próximo", "Next", "Weiter", "Следующий", "次へ", "下一个", "Siguiente", "अगला"),
    ("Iniciar", "Start", "Starten", "Запустить", "開始", "启动", "Iniciar", "शुरू करें"),
    ("Parar", "Stop", "Stoppen", "Остановить", "停止", "停止", "Detener", "रोकें"),
    ("Verificando serviço…", "Checking service…", "Dienst wird geprüft…", "Проверка службы…", "サービスを確認中…", "正在检查服务…", "Comprobando el servicio…", "सेवा की जाँच हो रही है…"),
    ("SUA BIBLIOTECA", "YOUR LIBRARY", "DEINE BIBLIOTHEK", "ВАША БИБЛИОТЕКА", "ライブラリ", "我的壁纸库", "TU BIBLIOTECA", "आपकी लाइब्रेरी"),
    ("EM REPRODUÇÃO AGORA", "NOW PLAYING", "LÄUFT GERADE", "СЕЙЧАС ВОСПРОИЗВОДИТСЯ", "再生中", "正在播放", "REPRODUCIENDO AHORA", "अभी चल रहा है"),
    ("Seus wallpapers, no seu ritmo.", "Your wallpapers, your rhythm.", "Deine Wallpaper, dein Rhythmus.", "Ваши обои — ваш ритм.", "壁紙を、あなたのペースで。", "你的壁纸，你的节奏。", "Tus fondos, a tu ritmo.", "आपके वॉलपेपर, आपकी रफ़्तार।"),
    ("Escolha um wallpaper ou monte uma playlist para começar.", "Choose a wallpaper or make a playlist to get started.", "Wähle ein Wallpaper oder erstelle eine Wiedergabeliste.", "Выберите обои или создайте плейлист.", "壁紙を選ぶか、プレイリストを作成して始めましょう。", "选择壁纸或创建播放列表以开始使用。", "Elige un fondo o crea una lista para empezar.", "शुरू करने के लिए वॉलपेपर चुनें या प्लेलिस्ट बनाएँ।"),
    ("Este wallpaper está ativo na sua área de trabalho.", "This wallpaper is active on your desktop.", "Dieses Wallpaper ist auf deinem Desktop aktiv.", "Эти обои активны на рабочем столе.", "この壁紙はデスクトップで使用中です。", "此壁纸正在桌面上使用。", "Este fondo está activo en tu escritorio.", "यह वॉलपेपर आपके डेस्कटॉप पर सक्रिय है।"),
    ("{count} instalados   ·   {favorites} favoritos   ·   {screens} telas", "{count} installed   ·   {favorites} favorites   ·   {screens} screens", "{count} installiert   ·   {favorites} Favoriten   ·   {screens} Bildschirme", "Установлено: {count}   ·   Избранное: {favorites}   ·   Экраны: {screens}", "インストール済み {count}   ·   お気に入り {favorites}   ·   画面 {screens}", "已安装 {count}   ·   收藏 {favorites}   ·   屏幕 {screens}", "{count} instalados   ·   {favorites} favoritos   ·   {screens} pantallas", "{count} इंस्टॉल   ·   {favorites} पसंदीदा   ·   {screens} स्क्रीन"),
    ("Buscar em seus wallpapers…  Ctrl+F", "Search your wallpapers…  Ctrl+F", "Wallpaper durchsuchen…  Strg+F", "Поиск по обоям…  Ctrl+F", "壁紙を検索…  Ctrl+F", "搜索壁纸…  Ctrl+F", "Buscar entre tus fondos…  Ctrl+F", "वॉलपेपर खोजें…  Ctrl+F"),
    ("Todos", "All", "Alle", "Все", "すべて", "全部", "Todos", "सभी"),
    ("Cenas", "Scenes", "Szenen", "Сцены", "シーン", "场景", "Escenas", "दृश्य"),
    ("Vídeos", "Videos", "Videos", "Видео", "動画", "视频", "Vídeos", "वीडियो"),
    ("Favoritos", "Favorites", "Favoriten", "Избранное", "お気に入り", "收藏", "Favoritos", "पसंदीदा"),
    ("Carregando…", "Loading…", "Wird geladen…", "Загрузка…", "読み込み中…", "加载中…", "Cargando…", "लोड हो रहा है…"),
    ("Clique para selecionar · Ctrl+clique para escolher vários · Shift+clique para um intervalo", "Click to select · Ctrl+click to select several · Shift+click for a range", "Klicken zum Auswählen · Strg+Klick für mehrere · Umschalt+Klick für einen Bereich", "Нажмите для выбора · Ctrl+щелчок для нескольких · Shift+щелчок для диапазона", "クリックで選択 · Ctrl+クリックで複数選択 · Shift+クリックで範囲選択", "点击选择 · Ctrl+点击多选 · Shift+点击选择范围", "Haz clic para seleccionar · Ctrl+clic para varios · Mayús+clic para un intervalo", "चुनने के लिए क्लिक करें · कई के लिए Ctrl+क्लिक · रेंज के लिए Shift+क्लिक"),
    ("Sua biblioteca está vazia", "Your library is empty", "Deine Bibliothek ist leer", "Ваша библиотека пуста", "ライブラリは空です", "壁纸库为空", "Tu biblioteca está vacía", "आपकी लाइब्रेरी खाली है"),
    ("Assine wallpapers no Wallpaper Engine original e aguarde o download pela Steam.", "Subscribe to wallpapers in the original Wallpaper Engine and wait for Steam to download them.", "Abonniere Wallpaper im originalen Wallpaper Engine und warte auf den Steam-Download.", "Подпишитесь на обои в оригинальном Wallpaper Engine и дождитесь загрузки в Steam.", "元の Wallpaper Engine で壁紙をサブスクライブし、Steam のダウンロードを待ってください。", "在原版 Wallpaper Engine 中订阅壁纸，并等待 Steam 下载。", "Suscríbete a fondos en el Wallpaper Engine original y espera a que Steam los descargue.", "मूल Wallpaper Engine में वॉलपेपर सब्सक्राइब करें और Steam डाउनलोड का इंतज़ार करें।"),
    ("Nada por aqui", "Nothing here", "Hier ist noch nichts", "Здесь пока ничего нет", "ここにはまだありません", "这里还没有内容", "Nada por aquí", "यहाँ कुछ नहीं है"),
    ("Tente outro termo ou altere o filtro para ver mais wallpapers.", "Try another search term or change the filter to see more wallpapers.", "Versuche einen anderen Suchbegriff oder ändere den Filter.", "Попробуйте другой запрос или измените фильтр.", "別のキーワードを試すか、フィルターを変更してください。", "尝试其他搜索词或更改筛选条件。", "Prueba otra búsqueda o cambia el filtro para ver más fondos.", "दूसरा शब्द खोजें या और वॉलपेपर देखने के लिए फ़िल्टर बदलें।"),
    ("{count} selecionado", "{count} selected", "{count} ausgewählt", "Выбрано: {count}", "{count} 件選択中", "已选择 {count} 项", "{count} seleccionado", "{count} चुना गया"),
    ("{count} selecionados", "{count} selected", "{count} ausgewählt", "Выбрано: {count}", "{count} 件選択中", "已选择 {count} 项", "{count} seleccionados", "{count} चुने गए"),
    ("0 selecionados", "0 selected", "0 ausgewählt", "Ничего не выбрано", "選択なし", "未选择任何项目", "0 seleccionados", "0 चुने गए"),
    ("Adicionar à playlist", "Add to playlist", "Zur Wiedergabeliste hinzufügen", "Добавить в плейлист", "プレイリストに追加", "添加到播放列表", "Añadir a la lista", "प्लेलिस्ट में जोड़ें"),
    ("ADICIONAR À PLAYLIST", "ADD TO PLAYLIST", "ZUR WIEDERGABELISTE HINZUFÜGEN", "ДОБАВИТЬ В ПЛЕЙЛИСТ", "プレイリストに追加", "添加到播放列表", "AÑADIR A LA LISTA", "प्लेलिस्ट में जोड़ें"),
    ("Limpar seleção", "Clear selection", "Auswahl aufheben", "Снять выделение", "選択を解除", "清除选择", "Borrar selección", "चयन हटाएँ"),
    ("Ainda não há playlists.", "No playlists yet.", "Noch keine Wiedergabelisten.", "Плейлистов пока нет.", "プレイリストはまだありません。", "还没有播放列表。", "Aún no hay listas.", "अभी कोई प्लेलिस्ट नहीं है।"),
    ("{visible} de {total}", "{visible} of {total}", "{visible} von {total}", "{visible} из {total}", "{visible} / {total}", "{visible} / {total}", "{visible} de {total}", "{total} में से {visible}"),
    ("{count} wallpapers", "{count} wallpapers", "{count} Wallpaper", "Обои: {count}", "壁紙 {count} 件", "{count} 张壁纸", "{count} fondos", "{count} वॉलपेपर"),
    ("{count} wallpapers instalados", "{count} installed wallpapers", "{count} installierte Wallpaper", "Установлено обоев: {count}", "インストール済みの壁紙 {count} 件", "已安装 {count} 张壁纸", "{count} fondos instalados", "{count} इंस्टॉल किए गए वॉलपेपर"),
    ("● AO VIVO", "● LIVE", "● AKTIV", "● АКТИВНО", "● 使用中", "● 使用中", "● EN VIVO", "● सक्रिय"),
    ("Cena", "Scene", "Szene", "Сцена", "シーン", "场景", "Escena", "दृश्य"),
    ("Vídeo", "Video", "Video", "Видео", "動画", "视频", "Vídeo", "वीडियो"),
    ("CENA", "SCENE", "SZENE", "СЦЕНА", "シーン", "场景", "ESCENA", "दृश्य"),
    ("VÍDEO", "VIDEO", "VIDEO", "ВИДЕО", "動画", "视频", "VÍDEO", "वीडियो"),
    ("{kind}  ·  {wallpaper_id}", "{kind}  ·  {wallpaper_id}", "{kind}  ·  {wallpaper_id}", "{kind}  ·  {wallpaper_id}", "{kind}  ·  {wallpaper_id}", "{kind}  ·  {wallpaper_id}", "{kind}  ·  {wallpaper_id}", "{kind}  ·  {wallpaper_id}"),
    ("{kind} · {wallpaper_id}", "{kind} · {wallpaper_id}", "{kind} · {wallpaper_id}", "{kind} · {wallpaper_id}", "{kind} · {wallpaper_id}", "{kind} · {wallpaper_id}", "{kind} · {wallpaper_id}", "{kind} · {wallpaper_id}"),
    ("Escolha um wallpaper na biblioteca para ver os detalhes e aplicar na tela.", "Choose a wallpaper in the library to see details and apply it to your screen.", "Wähle ein Wallpaper in der Bibliothek, um Details zu sehen und es anzuwenden.", "Выберите обои в библиотеке, чтобы увидеть подробности и применить их.", "ライブラリで壁紙を選ぶと、詳細の表示や画面への適用ができます。", "从壁纸库中选择壁纸，即可查看详情并应用到屏幕。", "Elige un fondo de la biblioteca para ver sus detalles y aplicarlo.", "विवरण देखने और स्क्रीन पर लगाने के लिए लाइब्रेरी से वॉलपेपर चुनें।"),
    ("WALLPAPER SELECIONADO", "SELECTED WALLPAPER", "AUSGEWÄHLTES WALLPAPER", "ВЫБРАННЫЕ ОБОИ", "選択中の壁紙", "已选壁纸", "FONDO SELECCIONADO", "चुना गया वॉलपेपर"),
    ("OPÇÕES DO WALLPAPER", "WALLPAPER OPTIONS", "WALLPAPER-OPTIONEN", "НАСТРОЙКИ ОБОЕВ", "壁紙のオプション", "壁纸选项", "OPCIONES DEL FONDO", "वॉलपेपर विकल्प"),
    ("Opções booleanas definidas pelo autor do wallpaper.", "Boolean options provided by the wallpaper author.", "Boolesche Optionen des Wallpaper-Autors.", "Логические параметры, заданные автором обоев.", "壁紙の作者が設定したオン・オフ項目です。", "壁纸作者提供的开关选项。", "Opciones de activación definidas por el autor del fondo.", "वॉलपेपर लेखक द्वारा दी गई चालू-बंद सेटिंग्स।"),
    ("ID {wallpaper_id}", "ID {wallpaper_id}", "ID {wallpaper_id}", "ID {wallpaper_id}", "ID {wallpaper_id}", "ID {wallpaper_id}", "ID {wallpaper_id}", "ID {wallpaper_id}"),
    ("● EM USO", "● IN USE", "● IN VERWENDUNG", "● ИСПОЛЬЗУЕТСЯ", "● 使用中", "● 使用中", "● EN USO", "● उपयोग में"),
    ("Aplicar agora", "Apply now", "Jetzt anwenden", "Применить сейчас", "今すぐ適用", "立即应用", "Aplicar ahora", "अभी लगाएँ"),
    ("Aplica em todas as telas e pausa a rotação. Fixações por monitor serão liberadas.", "Applies to every screen and pauses rotation. Per-screen assignments will be cleared.", "Wird auf allen Bildschirmen angezeigt und pausiert den Wechsel. Bildschirmzuweisungen werden entfernt.", "Применяется ко всем экранам и приостанавливает смену. Закрепления за экранами будут сняты.", "すべての画面に適用して切り替えを一時停止します。画面ごとの固定は解除されます。", "应用到所有屏幕并暂停轮换。各屏幕的固定设置将被清除。", "Se aplica a todas las pantallas y pausa la rotación. Se quitarán las asignaciones por pantalla.", "सभी स्क्रीन पर लगेगा और बदलाव रुकेगा। हर स्क्रीन पर तय किए गए वॉलपेपर हट जाएँगे।"),
    ("Remover dos favoritos", "Remove from favorites", "Aus Favoriten entfernen", "Убрать из избранного", "お気に入りから削除", "从收藏中移除", "Quitar de favoritos", "पसंदीदा से हटाएँ"),
    ("Adicionar aos favoritos", "Add to favorites", "Zu Favoriten hinzufügen", "Добавить в избранное", "お気に入りに追加", "添加到收藏", "Añadir a favoritos", "पसंदीदा में जोड़ें"),
    ("Adicionar", "Add", "Hinzufügen", "Добавить", "追加", "添加", "Añadir", "जोड़ें"),
    ("Criar primeira playlist", "Create first playlist", "Erste Wiedergabeliste erstellen", "Создать первый плейлист", "最初のプレイリストを作成", "创建第一个播放列表", "Crear la primera lista", "पहली प्लेलिस्ट बनाएँ"),
    ("TELAS", "SCREENS", "BILDSCHIRME", "ЭКРАНЫ", "画面", "屏幕", "PANTALLAS", "स्क्रीन"),
    ("Liberar", "Unpin", "Lösen", "Открепить", "固定を解除", "取消固定", "Liberar", "अनपिन करें"),
    ("Fixar aqui", "Pin here", "Hier fixieren", "Закрепить здесь", "ここに固定", "固定到此屏幕", "Fijar aquí", "यहाँ पिन करें"),
    ("Fixado: {title}", "Pinned: {title}", "Fixiert: {title}", "Закреплено: {title}", "固定中: {title}", "已固定：{title}", "Fijado: {title}", "पिन किया गया: {title}"),
    ("Suas playlists", "Your playlists", "Deine Wiedergabelisten", "Ваши плейлисты", "プレイリスト", "我的播放列表", "Tus listas de reproducción", "आपकी प्लेलिस्ट"),
    ("Rotação pela biblioteca", "Rotating from library", "Wechsel aus der Bibliothek", "Смена из библиотеки", "ライブラリから切り替え", "从壁纸库轮换", "Rotación desde la biblioteca", "लाइब्रेरी से वॉलपेपर बदलें"),
    ("Nova playlist", "New playlist", "Neue Wiedergabeliste", "Новый плейлист", "新しいプレイリスト", "新建播放列表", "Nueva lista", "नई प्लेलिस्ट"),
    ("Criar playlist", "Create playlist", "Wiedergabeliste erstellen", "Создать плейлист", "プレイリストを作成", "创建播放列表", "Crear lista", "प्लेलिस्ट बनाएँ"),
    ("Criar playlist · Ctrl+N", "Create playlist · Ctrl+N", "Wiedergabeliste erstellen · Strg+N", "Создать плейлист · Ctrl+N", "プレイリストを作成 · Ctrl+N", "创建播放列表 · Ctrl+N", "Crear lista · Ctrl+N", "प्लेलिस्ट बनाएँ · Ctrl+N"),
    ("Ativa: {name}", "Active: {name}", "Aktiv: {name}", "Активен: {name}", "有効: {name}", "当前：{name}", "Activa: {name}", "सक्रिय: {name}"),
    ("ATIVA", "ACTIVE", "AKTIV", "АКТИВЕН", "有効", "已启用", "ACTIVA", "सक्रिय"),
    ("COMECE POR AQUI", "START HERE", "HIER STARTEN", "НАЧНИТЕ ЗДЕСЬ", "ここから始める", "从这里开始", "EMPIEZA AQUÍ", "यहाँ से शुरू करें"),
    ("Uma trilha para cada clima.", "A playlist for every mood.", "Eine Wiedergabeliste für jede Stimmung.", "Плейлист под любое настроение.", "気分に合わせたプレイリスト。", "为每种心情创建播放列表。", "Una lista para cada estado de ánimo.", "हर मूड के लिए एक प्लेलिस्ट।"),
    ("Crie uma playlist e escolha vários wallpapers instalados de uma vez. Você decide a ordem ou deixa a reprodução aleatória.", "Create a playlist and choose several installed wallpapers at once. Set the order or play them at random.", "Erstelle eine Wiedergabeliste und wähle mehrere installierte Wallpaper aus. Bestimme die Reihenfolge oder nutze Zufall.", "Создайте плейлист и выберите сразу несколько установленных обоев. Задайте порядок или включите случайное воспроизведение.", "プレイリストを作り、インストール済みの壁紙をまとめて選べます。順番を決めるか、ランダム再生にできます。", "创建播放列表并一次选择多张已安装壁纸。你可以指定顺序或随机播放。", "Crea una lista y elige varios fondos instalados a la vez. Decide el orden o usa la reproducción aleatoria.", "प्लेलिस्ट बनाएँ और एक साथ कई इंस्टॉल किए गए वॉलपेपर चुनें। क्रम तय करें या रैंडम चलाएँ।"),
    ("Criar minha primeira playlist", "Create my first playlist", "Meine erste Wiedergabeliste erstellen", "Создать первый плейлист", "最初のプレイリストを作成", "创建我的第一个播放列表", "Crear mi primera lista", "मेरी पहली प्लेलिस्ट बनाएँ"),
    ("EDITOR DE PLAYLIST", "PLAYLIST EDITOR", "WIEDERGABELISTEN-EDITOR", "РЕДАКТОР ПЛЕЙЛИСТА", "プレイリスト編集", "播放列表编辑器", "EDITOR DE LISTAS", "प्लेलिस्ट संपादक"),
    ("Renomear", "Rename", "Umbenennen", "Переименовать", "名前を変更", "重命名", "Cambiar nombre", "नाम बदलें"),
    ("Renomear playlist", "Rename playlist", "Wiedergabeliste umbenennen", "Переименовать плейлист", "プレイリスト名を変更", "重命名播放列表", "Cambiar nombre de la lista", "प्लेलिस्ट का नाम बदलें"),
    ("Excluir", "Delete", "Löschen", "Удалить", "削除", "删除", "Eliminar", "हटाएँ"),
    ("{count} itens · ", "{count} items · ", "{count} Elemente · ", "Элементов: {count} · ", "{count} 件 · ", "{count} 项 · ", "{count} elementos · ", "{count} आइटम · "),
    ("Selecionada para a rotação", "Selected for rotation", "Für den Wechsel ausgewählt", "Выбран для смены обоев", "切り替え対象に選択中", "已选为轮换列表", "Seleccionada para la rotación", "वॉलपेपर बदलने के लिए चुनी गई"),
    ("Selecionada, com rotação pausada", "Selected, rotation paused", "Ausgewählt, Wechsel pausiert", "Выбран, смена приостановлена", "選択中、切り替えは一時停止", "已选择，轮换已暂停", "Seleccionada, rotación en pausa", "चुनी गई, बदलाव रुका हुआ है"),
    ("Pronta para ativar", "Ready to activate", "Bereit zum Aktivieren", "Готов к включению", "有効化できます", "可随时启用", "Lista para activar", "सक्रिय करने के लिए तैयार"),
    ("Ativar e iniciar rotação", "Activate and start rotation", "Aktivieren und Wechsel starten", "Включить и начать смену", "有効にして切り替えを開始", "启用并开始轮换", "Activar e iniciar rotación", "सक्रिय कर बदलाव शुरू करें"),
    ("Voltar à biblioteca na rotação", "Return to library rotation", "Zurück zum Bibliothekswechsel", "Вернуться к смене из библиотеки", "ライブラリからの切り替えに戻る", "返回壁纸库轮换", "Volver a la rotación de la biblioteca", "लाइब्रेरी से बदलाव पर लौटें"),
    ("A ordem abaixo vale quando a opção aleatória está desligada. Itens salvos que não estão instalados serão ignorados até reaparecerem na biblioteca.", "The order below applies when shuffle is off. Saved items that are not installed are skipped until they reappear in the library.", "Die Reihenfolge unten gilt bei deaktiviertem Zufall. Gespeicherte, nicht installierte Elemente werden übersprungen, bis sie wieder in der Bibliothek erscheinen.", "Порядок ниже действует при выключенном перемешивании. Сохранённые, но не установленные обои пропускаются до возвращения в библиотеку.", "以下の順番はシャッフルをオフにしたときに使われます。未インストールの保存済み壁紙は、ライブラリに戻るまでスキップされます。", "关闭随机播放时将按以下顺序轮换。未安装的已保存项目会被跳过，直到重新出现在壁纸库中。", "El orden siguiente se usa cuando la reproducción aleatoria está desactivada. Los elementos guardados que no estén instalados se omiten hasta que vuelvan a la biblioteca.", "शफ़ल बंद होने पर नीचे का क्रम लागू होता है। इंस्टॉल न किए गए सहेजे हुए आइटम, लाइब्रेरी में लौटने तक छोड़े जाएँगे।"),
    ("Adicionar wallpapers", "Add wallpapers", "Wallpaper hinzufügen", "Добавить обои", "壁紙を追加", "添加壁纸", "Añadir fondos", "वॉलपेपर जोड़ें"),
    ("Adicionar selecionado: {title}", "Add selected: {title}", "Ausgewähltes hinzufügen: {title}", "Добавить выбранное: {title}", "選択中の壁紙を追加: {title}", "添加已选壁纸：{title}", "Añadir seleccionado: {title}", "चुना गया जोड़ें: {title}"),
    ("ORDEM DE REPRODUÇÃO", "PLAYBACK ORDER", "WIEDERGABEREIHENFOLGE", "ПОРЯДОК ВОСПРОИЗВЕДЕНИЯ", "再生順", "播放顺序", "ORDEN DE REPRODUCCIÓN", "चलाने का क्रम"),
    ("Esta playlist ainda está vazia. Adicione wallpapers instalados para começar.", "This playlist is empty. Add installed wallpapers to get started.", "Diese Wiedergabeliste ist leer. Füge installierte Wallpaper hinzu.", "Этот плейлист пуст. Добавьте установленные обои.", "このプレイリストは空です。インストール済みの壁紙を追加してください。", "此播放列表为空。添加已安装的壁纸即可开始。", "Esta lista está vacía. Añade fondos instalados para empezar.", "यह प्लेलिस्ट खाली है। शुरू करने के लिए इंस्टॉल किए गए वॉलपेपर जोड़ें।"),
    ("{title} · não instalado", "{title} · not installed", "{title} · nicht installiert", "{title} · не установлено", "{title} · 未インストール", "{title} · 未安装", "{title} · no instalado", "{title} · इंस्टॉल नहीं है"),
    ("{position}. {title}", "{position}. {title}", "{position}. {title}", "{position}. {title}", "{position}. {title}", "{position}. {title}", "{position}. {title}", "{position}. {title}"),
    ("Mover para cima", "Move up", "Nach oben verschieben", "Переместить вверх", "上に移動", "上移", "Subir", "ऊपर ले जाएँ"),
    ("Mover para baixo", "Move down", "Nach unten verschieben", "Переместить вниз", "下に移動", "下移", "Bajar", "नीचे ले जाएँ"),
    ("Remover", "Remove", "Entfernen", "Убрать", "削除", "移除", "Quitar", "हटाएँ"),
    ("Cancelar", "Cancel", "Abbrechen", "Отмена", "キャンセル", "取消", "Cancelar", "रद्द करें"),
    ("Salvar", "Save", "Speichern", "Сохранить", "保存", "保存", "Guardar", "सहेजें"),
    ("Nome da playlist", "Playlist name", "Name der Wiedergabeliste", "Название плейлиста", "プレイリスト名", "播放列表名称", "Nombre de la lista", "प्लेलिस्ट का नाम"),
    ("Já existe uma playlist com esse nome.", "A playlist with that name already exists.", "Eine Wiedergabeliste mit diesem Namen gibt es bereits.", "Плейлист с таким названием уже существует.", "その名前のプレイリストは既にあります。", "此名称的播放列表已存在。", "Ya existe una lista con ese nombre.", "इस नाम की प्लेलिस्ट पहले से मौजूद है।"),
    ("Excluir playlist", "Delete playlist", "Wiedergabeliste löschen", "Удалить плейлист", "プレイリストを削除", "删除播放列表", "Eliminar lista", "प्लेलिस्ट हटाएँ"),
    ("Excluir ‘{name}’? Os wallpapers instalados continuarão na biblioteca.", "Delete ‘{name}’? Installed wallpapers will remain in the library.", "‘{name}’ löschen? Installierte Wallpaper bleiben in der Bibliothek.", "Удалить «{name}»? Установленные обои останутся в библиотеке.", "「{name}」を削除しますか？インストール済みの壁紙はライブラリに残ります。", "删除“{name}”？已安装的壁纸仍会保留在壁纸库中。", "¿Eliminar «{name}»? Los fondos instalados seguirán en la biblioteca.", "‘{name}’ हटाएँ? इंस्टॉल किए गए वॉलपेपर लाइब्रेरी में रहेंगे।"),
    ("Todos os wallpapers instalados já estão nesta playlist.", "All installed wallpapers are already in this playlist.", "Alle installierten Wallpaper sind bereits in dieser Wiedergabeliste.", "Все установленные обои уже в этом плейлисте.", "インストール済みの壁紙はすべてこのプレイリストにあります。", "所有已安装壁纸都已在此播放列表中。", "Todos los fondos instalados ya están en esta lista.", "इंस्टॉल किए गए सभी वॉलपेपर पहले से इस प्लेलिस्ट में हैं।"),
    ("Adicionar a {name}", "Add to {name}", "Zu {name} hinzufügen", "Добавить в {name}", "{name} に追加", "添加到 {name}", "Añadir a {name}", "{name} में जोड़ें"),
    ("Adicionar selecionados", "Add selected", "Ausgewählte hinzufügen", "Добавить выбранное", "選択した項目を追加", "添加所选项目", "Añadir seleccionados", "चुने गए जोड़ें"),
    ("Escolha vários wallpapers instalados", "Choose multiple installed wallpapers", "Mehrere installierte Wallpaper auswählen", "Выберите несколько установленных обоев", "インストール済みの壁紙を複数選択", "选择多张已安装壁纸", "Elige varios fondos instalados", "कई इंस्टॉल किए गए वॉलपेपर चुनें"),
    ("Buscar por nome, tag ou ID", "Search by name, tag or ID", "Nach Name, Tag oder ID suchen", "Поиск по названию, тегу или ID", "名前、タグ、ID で検索", "按名称、标签或 ID 搜索", "Buscar por nombre, etiqueta o ID", "नाम, टैग या ID से खोजें"),
    ("Nenhum selecionado", "Nothing selected", "Nichts ausgewählt", "Ничего не выбрано", "選択なし", "未选择任何项目", "Ninguno seleccionado", "कुछ भी चुना नहीं गया"),
    ("Selecionar {title}", "Select {title}", "{title} auswählen", "Выбрать {title}", "{title} を選択", "选择 {title}", "Seleccionar {title}", "{title} चुनें"),
    ("Este wallpaper já está na playlist.", "This wallpaper is already in the playlist.", "Dieses Wallpaper ist bereits in der Wiedergabeliste.", "Эти обои уже есть в плейлисте.", "この壁紙は既にプレイリストにあります。", "此壁纸已在播放列表中。", "Este fondo ya está en la lista.", "यह वॉलपेपर पहले से प्लेलिस्ट में है।"),
    ("01 · REPRODUÇÃO", "01 · PLAYBACK", "01 · WIEDERGABE", "01 · ВОСПРОИЗВЕДЕНИЕ", "01 · 再生", "01 · 播放", "01 · REPRODUCCIÓN", "01 · प्लेबैक"),
    ("Uma coleção sempre em movimento", "A collection always in motion", "Eine Sammlung, die immer in Bewegung ist", "Коллекция всегда в движении", "いつも動き続けるコレクション", "让你的壁纸始终鲜活", "Una colección siempre en movimiento", "हमेशा चलती रहने वाली कलेक्शन"),
    ("Controle a troca automática e escolha de onde vêm os próximos wallpapers.", "Control automatic rotation and choose where the next wallpapers come from.", "Steuere den automatischen Wechsel und wähle die Quelle für die nächsten Wallpaper.", "Управляйте автоматической сменой и источником следующих обоев.", "自動切り替えと次の壁紙の選択元を設定します。", "控制自动轮换并选择下一张壁纸的来源。", "Controla la rotación automática y elige de dónde salen los próximos fondos.", "स्वचालित बदलाव और अगले वॉलपेपर का स्रोत चुनें।"),
    ("Troca automática", "Automatic rotation", "Automatischer Wechsel", "Автоматическая смена", "自動切り替え", "自动轮换", "Rotación automática", "स्वचालित बदलाव"),
    ("Escolhe outro wallpaper após o intervalo.", "Choose another wallpaper after the interval.", "Wählt nach Ablauf des Intervalls ein anderes Wallpaper.", "Выбирает другие обои по истечении интервала.", "設定した間隔で次の壁紙に切り替えます。", "经过设定的时间后选择另一张壁纸。", "Elige otro fondo al terminar el intervalo.", "तय समय के बाद दूसरा वॉलपेपर चुनता है।"),
    ("Usar só favoritos", "Favorites only", "Nur Favoriten verwenden", "Только избранное", "お気に入りのみ", "仅使用收藏", "Usar solo favoritos", "सिर्फ़ पसंदीदा"),
    ("Limita a rotação aos wallpapers marcados.", "Rotate only through marked wallpapers.", "Wechselt nur zwischen markierten Wallpapern.", "Смена только избранных обоев.", "お気に入りの壁紙だけを切り替えます。", "仅在已收藏的壁纸间轮换。", "Limita la rotación a los fondos marcados.", "केवल चुने गए वॉलपेपर के बीच बदलेगा।"),
    ("Ordem aleatória", "Shuffle", "Zufällige Reihenfolge", "Случайный порядок", "シャッフル", "随机顺序", "Orden aleatorio", "रैंडम क्रम"),
    ("Embaralha os wallpapers elegíveis.", "Shuffle eligible wallpapers.", "Mischt die verfügbaren Wallpaper.", "Перемешивает доступные обои.", "対象の壁紙をシャッフルします。", "随机轮换可用壁纸。", "Mezcla los fondos disponibles.", "उपलब्ध वॉलपेपर रैंडम क्रम में चलाता है।"),
    ("Intervalo entre trocas", "Time between changes", "Intervall zwischen Wechseln", "Интервал смены", "切り替え間隔", "轮换间隔", "Intervalo entre cambios", "बदलाव के बीच का समय"),
    ("Minutos", "Minutes", "Minuten", "Минуты", "分", "分钟", "Minutos", "मिनट"),
    ("02 · MOTOR", "02 · ENGINE", "02 · ENGINE", "02 · ДВИЖОК", "02 · エンジン", "02 · 引擎", "02 · MOTOR", "02 · इंजन"),
    ("Imagem e desempenho", "Image and performance", "Bild und Leistung", "Изображение и производительность", "画質とパフォーマンス", "画面与性能", "Imagen y rendimiento", "तस्वीर और प्रदर्शन"),
    ("Ajustes que afetam a renderização dos wallpapers em cada tela.", "Settings that affect wallpaper rendering on each screen.", "Einstellungen für die Darstellung auf jedem Bildschirm.", "Настройки отображения обоев на каждом экране.", "各画面での壁紙の描画を設定します。", "影响每个屏幕壁纸渲染的设置。", "Ajustes que afectan la representación de los fondos en cada pantalla.", "हर स्क्रीन पर वॉलपेपर दिखने का तरीका तय करने वाली सेटिंग्स।"),
    ("Limite de quadros", "Frame limit", "Bildratenlimit", "Ограничение кадров", "フレームレート上限", "帧率上限", "Límite de fotogramas", "फ़्रेम सीमा"),
    ("FPS", "FPS", "FPS", "FPS", "FPS", "FPS", "FPS", "FPS"),
    ("Escala", "Scaling", "Skalierung", "Масштабирование", "拡大縮小", "缩放", "Escala", "स्केलिंग"),
    ("Como a imagem ocupa a tela.", "How the image fills the screen.", "Wie das Bild den Bildschirm ausfüllt.", "Как изображение заполняет экран.", "画面に画像を合わせる方法です。", "设置图像如何填充屏幕。", "Cómo ocupa la imagen la pantalla.", "तस्वीर स्क्रीन में कैसे फ़िट होती है।"),
    ("Preencher", "Fill", "Ausfüllen", "Заполнить", "画面いっぱい", "填充", "Rellenar", "भरें"),
    ("Ajustar", "Fit", "Einpassen", "Вместить", "全体を表示", "适应", "Ajustar", "फ़िट करें"),
    ("Esticar", "Stretch", "Strecken", "Растянуть", "引き伸ばす", "拉伸", "Estirar", "खींचें"),
    ("Padrão", "Default", "Standard", "По умолчанию", "既定", "默认", "Predeterminado", "डिफ़ॉल्ट"),
    ("Silenciar", "Mute", "Stummschalten", "Без звука", "ミュート", "静音", "Silenciar", "म्यूट"),
    ("Executa o renderizador sem áudio.", "Run the renderer without audio.", "Startet den Renderer ohne Ton.", "Запускает рендерер без звука.", "音声なしでレンダラーを実行します。", "运行渲染器时不输出声音。", "Ejecuta el motor sin sonido.", "रेंडरर को बिना आवाज़ चलाता है।"),
    ("03 · APLICATIVO", "03 · APPLICATION", "03 · ANWENDUNG", "03 · ПРИЛОЖЕНИЕ", "03 · アプリ", "03 · 应用", "03 · APLICACIÓN", "03 · ऐप"),
    ("Sempre pronto quando você entrar", "Ready whenever you sign in", "Bereit, sobald du dich anmeldest", "Готово после входа в систему", "ログイン時に自動で準備", "登录后即可使用", "Siempre listo al iniciar sesión", "लॉग इन करते ही तैयार"),
    ("Configure o serviço e o caminho do motor instalado neste computador.", "Configure the service and the installed engine path on this computer.", "Konfiguriere den Dienst und den Pfad zur installierten Engine.", "Настройте службу и путь к установленному движку.", "サービスと、このコンピューターにインストールされたエンジンの場所を設定します。", "配置服务和本机已安装引擎的路径。", "Configura el servicio y la ruta del motor instalado en este equipo.", "इस कंप्यूटर पर सेवा और इंस्टॉल किए गए इंजन का पथ तय करें।"),
    ("Iniciar com a sessão", "Start with session", "Mit der Sitzung starten", "Запускать при входе", "ログイン時に起動", "登录时启动", "Iniciar con la sesión", "सेशन के साथ शुरू करें"),
    ("Ativa o serviço do usuário quando você entra no KDE.", "Enable the user service when you sign in to KDE.", "Aktiviert den Benutzerdienst bei der KDE-Anmeldung.", "Включает пользовательскую службу при входе в KDE.", "KDE へのログイン時にユーザーサービスを有効にします。", "登录 KDE 时启用用户服务。", "Activa el servicio de usuario al entrar en KDE.", "KDE में लॉग इन करते ही उपयोगकर्ता सेवा चालू करता है।"),
    ("Detectar linux-wallpaperengine no PATH", "Find linux-wallpaperengine in PATH", "linux-wallpaperengine im PATH finden", "Найти linux-wallpaperengine в PATH", "PATH から linux-wallpaperengine を検出", "在 PATH 中查找 linux-wallpaperengine", "Detectar linux-wallpaperengine en PATH", "PATH में linux-wallpaperengine खोजें"),
    ("Executável do motor", "Engine executable", "Engine-Programm", "Исполняемый файл движка", "エンジンの実行ファイル", "引擎可执行文件", "Ejecutable del motor", "इंजन की एक्ज़िक्यूटेबल फ़ाइल"),
    ("Caminho personalizado, se necessário.", "Custom path, if needed.", "Benutzerdefinierter Pfad, falls nötig.", "Укажите другой путь при необходимости.", "必要に応じて独自のパスを指定します。", "如有需要，可指定自定义路径。", "Ruta personalizada, si hace falta.", "ज़रूरत हो तो अपना पथ दें।"),
    ("Descubra e assine novos itens no Wallpaper Engine original. A Steam faz o download; este app acompanha sua biblioteca local. O motor continua renderizando quando outra janela cobre o wallpaper, preservando a estabilidade do vídeo e do áudio da sessão.", "Discover and subscribe to new items in the original Wallpaper Engine. Steam downloads them; this app follows your local library. The engine keeps rendering when another window covers the wallpaper to preserve video and session audio stability.", "Entdecke und abonniere neue Inhalte im originalen Wallpaper Engine. Steam lädt sie herunter; diese App verfolgt deine lokale Bibliothek. Die Engine rendert weiter, wenn ein Fenster das Wallpaper verdeckt, um Video und Sitzungs-Audio stabil zu halten.", "Находите новые обои и подписывайтесь на них в оригинальном Wallpaper Engine. Steam загрузит их, а это приложение покажет локальную библиотеку. Движок продолжает работу под другими окнами для стабильности видео и звука в сеансе.", "新しい作品は元の Wallpaper Engine で探してサブスクライブしてください。Steam がダウンロードし、このアプリがローカルライブラリを追跡します。他のウィンドウが壁紙を覆っても描画を続け、動画とセッション音声を安定させます。", "请在原版 Wallpaper Engine 中发现并订阅新项目。Steam 会下载它们；本应用会同步本地壁纸库。其他窗口遮挡壁纸时，引擎仍继续渲染，以保持视频和会话音频稳定。", "Descubre y suscríbete a nuevos elementos en el Wallpaper Engine original. Steam los descargará y esta aplicación seguirá tu biblioteca local. El motor seguirá renderizando cuando otra ventana cubra el fondo para mantener estables el vídeo y el audio de la sesión.", "नए आइटम मूल Wallpaper Engine में खोजें और सब्सक्राइब करें। Steam उन्हें डाउनलोड करेगा; यह ऐप आपकी स्थानीय लाइब्रेरी दिखाएगा। दूसरी विंडो वॉलपेपर को ढकने पर भी इंजन चलता रहेगा, ताकि वीडियो और सेशन ऑडियो स्थिर रहें।"),
    ("Aplica em todas as telas, interrompe a playlist ativa e libera fixações por monitor.", "Applies to every screen, stops the active playlist, and clears per-screen assignments.", "Wird auf allen Bildschirmen angezeigt, stoppt die aktive Wiedergabeliste und löst Bildschirmzuweisungen.", "Применяется ко всем экранам, останавливает активный плейлист и снимает закрепления за экранами.", "すべての画面に適用し、アクティブなプレイリストを停止して画面ごとの固定を解除します。", "应用到所有屏幕，停止当前播放列表并清除各屏幕的固定设置。", "Se aplica a todas las pantallas, detiene la lista activa y quita las asignaciones por pantalla.", "सभी स्क्रीन पर लगाता है, सक्रिय प्लेलिस्ट रोकता है और हर स्क्रीन की पिनिंग हटाता है।"),
    ("Erro ao ler wallpapers: {error}", "Could not read wallpapers: {error}", "Wallpaper konnten nicht gelesen werden: {error}", "Не удалось прочитать обои: {error}", "壁紙を読み込めませんでした: {error}", "无法读取壁纸：{error}", "No se pudieron leer los fondos: {error}", "वॉलपेपर नहीं पढ़े जा सके: {error}"),
    ("Falha na leitura", "Read failed", "Lesen fehlgeschlagen", "Ошибка чтения", "読み込みに失敗", "读取失败", "Error de lectura", "पढ़ने में विफल"),
    ("Não foi possível executar {command}: {error}", "Could not run {command}: {error}", "{command} konnte nicht ausgeführt werden: {error}", "Не удалось выполнить {command}: {error}", "{command} を実行できませんでした: {error}", "无法执行 {command}：{error}", "No se pudo ejecutar {command}: {error}", "{command} नहीं चल सका: {error}"),
    ("Alteração aplicada.", "Change applied.", "Änderung übernommen.", "Изменение применено.", "変更を適用しました。", "已应用更改。", "Cambio aplicado.", "बदलाव लागू हो गया।"),
    ("Serviço indisponível: {error}", "Service unavailable: {error}", "Dienst nicht verfügbar: {error}", "Служба недоступна: {error}", "サービスを利用できません: {error}", "服务不可用：{error}", "Servicio no disponible: {error}", "सेवा उपलब्ध नहीं है: {error}"),
    ("Serviço parado ou ainda não instalado", "Service stopped or not installed yet", "Dienst gestoppt oder noch nicht installiert", "Служба остановлена или ещё не установлена", "サービスが停止中か未インストールです", "服务已停止或尚未安装", "Servicio detenido o aún no instalado", "सेवा बंद है या अभी इंस्टॉल नहीं हुई है"),
    ("Em execução: {title}", "Running: {title}", "Läuft: {title}", "Работает: {title}", "実行中: {title}", "正在运行：{title}", "En ejecución: {title}", "चल रहा है: {title}"),
    ("Aguardando: {error}", "Waiting: {error}", "Warten: {error}", "Ожидание: {error}", "待機中: {error}", "等待中：{error}", "Esperando: {error}", "इंतज़ार है: {error}"),
    ("Iniciando wallpaper…", "Starting wallpaper…", "Wallpaper wird gestartet…", "Запуск обоев…", "壁紙を開始中…", "正在启动壁纸…", "Iniciando fondo…", "वॉलपेपर शुरू हो रहा है…"),
    ("Serviço ativo · wallpaper parado", "Service active · wallpaper stopped", "Dienst aktiv · Wallpaper gestoppt", "Служба работает · обои остановлены", "サービスは稼働中 · 壁紙は停止中", "服务运行中 · 壁纸已停止", "Servicio activo · fondo detenido", "सेवा सक्रिय · वॉलपेपर बंद"),
    ("Próxima troca em {minutes:02d}:{seconds:02d}", "Next change in {minutes:02d}:{seconds:02d}", "Nächster Wechsel in {minutes:02d}:{seconds:02d}", "Следующая смена через {minutes:02d}:{seconds:02d}", "次の切り替えまで {minutes:02d}:{seconds:02d}", "距下次轮换 {minutes:02d}:{seconds:02d}", "Próximo cambio en {minutes:02d}:{seconds:02d}", "अगला बदलाव {minutes:02d}:{seconds:02d} में"),
    ("Não foi possível {verb} o serviço: {error}", "Could not {verb} the service: {error}", "Dienst konnte nicht {verb} werden: {error}", "Не удалось выполнить {verb} для службы: {error}", "サービスの {verb} に失敗しました: {error}", "无法对服务执行 {verb}：{error}", "No se pudo {verb} el servicio: {error}", "सेवा पर {verb} नहीं कर सके: {error}"),
    ("systemctl falhou", "systemctl failed", "systemctl fehlgeschlagen", "Ошибка systemctl", "systemctl に失敗しました", "systemctl 执行失败", "systemctl falló", "systemctl विफल रहा"),
    ("Iniciar serviço", "Start service", "Dienst starten", "Запустить службу", "サービスを開始", "启动服务", "Iniciar servicio", "सेवा शुरू करें"),
    ("Tentar novamente", "Try again", "Erneut versuchen", "Повторить", "再試行", "重试", "Intentar de nuevo", "फिर कोशिश करें"),
    ("Voltar", "Back", "Zurück", "Назад", "戻る", "返回", "Volver", "वापस"),
    ("Selecionar tudo", "Select all", "Alles auswählen", "Выбрать всё", "すべて選択", "全选", "Seleccionar todo", "सभी चुनें"),
    ("Selecionar wallpapers", "Select wallpapers", "Wallpaper auswählen", "Выбрать обои", "壁紙を選択", "选择壁纸", "Seleccionar fondos", "वॉलपेपर चुनें"),
    ("Filtros", "Filters", "Filter", "Фильтры", "フィルター", "筛选", "Filtros", "फ़िल्टर"),
    ("Nenhum resultado", "No results", "Keine Ergebnisse", "Нет результатов", "結果がありません", "没有结果", "Sin resultados", "कोई नतीजा नहीं"),
    ("Nenhuma tela detectada", "No screen detected", "Kein Bildschirm erkannt", "Экраны не обнаружены", "画面が見つかりません", "未检测到屏幕", "No se detectó ninguna pantalla", "कोई स्क्रीन नहीं मिली"),
    ("Biblioteca local", "Local library", "Lokale Bibliothek", "Локальная библиотека", "ローカルライブラリ", "本地壁纸库", "Biblioteca local", "स्थानीय लाइब्रेरी"),
    ("Wallpaper em uso", "Wallpaper in use", "Wallpaper in Verwendung", "Обои используются", "使用中の壁紙", "使用中的壁纸", "Fondo en uso", "उपयोग में वॉलपेपर"),
    ("Renderizador linux-wallpaperengine não encontrado ou sem permissão de execução.", "The linux-wallpaperengine renderer was not found or is not executable.", "Der Renderer linux-wallpaperengine wurde nicht gefunden oder ist nicht ausführbar.", "Рендерер linux-wallpaperengine не найден или недоступен для запуска.", "レンダラー linux-wallpaperengine が見つからないか、実行権限がありません。", "找不到 linux-wallpaperengine 渲染器，或其没有执行权限。", "No se encontró el motor linux-wallpaperengine o no tiene permiso de ejecución.", "linux-wallpaperengine रेंडरर नहीं मिला या उसे चलाने की अनुमति नहीं है।"),
    ("Wallpaper selecionado não está mais instalado.", "The selected wallpaper is no longer installed.", "Das ausgewählte Wallpaper ist nicht mehr installiert.", "Выбранные обои больше не установлены.", "選択した壁紙はもうインストールされていません。", "所选壁纸已不再安装。", "El fondo seleccionado ya no está instalado.", "चुना गया वॉलपेपर अब इंस्टॉल नहीं है।"),
    ("Nenhum monitor ativo detectado.", "No active monitor detected.", "Kein aktiver Bildschirm erkannt.", "Активные мониторы не обнаружены.", "アクティブなモニターが見つかりません。", "未检测到活动显示器。", "No se detectó ningún monitor activo.", "कोई सक्रिय मॉनिटर नहीं मिला।"),
    ("O renderizador encerrou (código {code}); nova tentativa em {delay} segundos.", "The renderer exited (code {code}); retrying in {delay} seconds.", "Der Renderer wurde beendet (Code {code}); neuer Versuch in {delay} Sekunden.", "Рендерер завершился (код {code}); повторная попытка через {delay} с.", "レンダラーが終了しました (コード {code})。{delay} 秒後に再試行します。", "渲染器已退出（代码 {code}）；{delay} 秒后重试。", "El motor terminó (código {code}); se reintentará en {delay} segundos.", "रेंडरर बंद हो गया (कोड {code}); {delay} सेकंड में फिर कोशिश होगी।"),
    ("A playlist '{playlist}' não contém wallpapers instalados e disponíveis para rotação.", "The playlist '{playlist}' has no installed wallpapers available for rotation.", "Die Wiedergabeliste '{playlist}' enthält keine installierten Wallpaper für den Wechsel.", "В плейлисте '{playlist}' нет установленных обоев для смены.", "プレイリスト「{playlist}」に切り替え可能なインストール済み壁紙がありません。", "播放列表“{playlist}”中没有可轮换的已安装壁纸。", "La lista '{playlist}' no contiene fondos instalados disponibles para la rotación.", "प्लेलिस्ट '{playlist}' में बदलाव के लिए कोई इंस्टॉल किया हुआ वॉलपेपर उपलब्ध नहीं है।"),
    ("Nenhum wallpaper elegível encontrado. Confira as assinaturas da Steam e o filtro de favoritos.", "No eligible wallpaper found. Check Steam subscriptions and the favorites filter.", "Kein geeignetes Wallpaper gefunden. Prüfe Steam-Abonnements und den Favoritenfilter.", "Подходящие обои не найдены. Проверьте подписки Steam и фильтр избранного.", "対象の壁紙が見つかりません。Steam のサブスクライブとお気に入りフィルターを確認してください。", "未找到可用壁纸。请检查 Steam 订阅和收藏筛选条件。", "No se encontró ningún fondo disponible. Comprueba las suscripciones de Steam y el filtro de favoritos.", "कोई उपलब्ध वॉलपेपर नहीं मिला। Steam सब्सक्रिप्शन और पसंदीदा फ़िल्टर जाँचें।"),
    ("Nenhum wallpaper scene ou video do Workshop encontrado.", "No scene or video wallpaper from the Workshop was found.", "Kein Szenen- oder Video-Wallpaper aus dem Workshop gefunden.", "Обои типа scene или video из Мастерской не найдены.", "Workshop にシーンまたは動画の壁紙が見つかりません。", "未找到来自创意工坊的场景或视频壁纸。", "No se encontraron fondos de escena o vídeo del Workshop.", "Workshop से कोई सीन या वीडियो वॉलपेपर नहीं मिला।"),
    ("Wallpaper {wallpaper_id} não está instalado.", "Wallpaper {wallpaper_id} is not installed.", "Wallpaper {wallpaper_id} ist nicht installiert.", "Обои {wallpaper_id} не установлены.", "壁紙 {wallpaper_id} はインストールされていません。", "壁纸 {wallpaper_id} 未安装。", "El fondo {wallpaper_id} no está instalado.", "वॉलपेपर {wallpaper_id} इंस्टॉल नहीं है।"),
    ("Solicitação inválida.", "Invalid request.", "Ungültige Anfrage.", "Недопустимый запрос.", "無効なリクエストです。", "无效请求。", "Solicitud no válida.", "अमान्य अनुरोध।"),
    ("Comando inválido.", "Invalid command.", "Ungültiger Befehl.", "Недопустимая команда.", "無効なコマンドです。", "无效命令。", "Comando no válido.", "अमान्य कमांड।"),
    ("Comando desconhecido: {command}.", "Unknown command: {command}.", "Unbekannter Befehl: {command}.", "Неизвестная команда: {command}.", "不明なコマンド: {command}。", "未知命令：{command}。", "Comando desconocido: {command}.", "अज्ञात कमांड: {command}।"),
    ("Argumentos inválidos para {command}.", "Invalid arguments for {command}.", "Ungültige Argumente für {command}.", "Недопустимые аргументы для {command}.", "{command} の引数が無効です。", "{command} 的参数无效。", "Argumentos no válidos para {command}.", "{command} के लिए अमान्य आर्ग्यूमेंट।"),
    ("A playlist ativa não contém wallpapers instalados e disponíveis.", "The active playlist has no installed wallpapers available.", "Die aktive Wiedergabeliste enthält keine verfügbaren installierten Wallpaper.", "В активном плейлисте нет доступных установленных обоев.", "アクティブなプレイリストに使用可能なインストール済み壁紙がありません。", "当前播放列表中没有可用的已安装壁纸。", "La lista activa no tiene fondos instalados disponibles.", "सक्रिय प्लेलिस्ट में कोई इंस्टॉल किया हुआ वॉलपेपर उपलब्ध नहीं है।"),
    ("Nenhum wallpaper elegível para avançar.", "No eligible wallpaper to advance to.", "Kein geeignetes Wallpaper zum Weiterschalten.", "Нет доступных обоев для перехода.", "次に切り替えられる壁紙がありません。", "没有可切换到的壁纸。", "No hay ningún fondo disponible para avanzar.", "आगे जाने के लिए कोई उपलब्ध वॉलपेपर नहीं है।"),
    ("As configurações devem ser um objeto.", "Settings must be an object.", "Einstellungen müssen ein Objekt sein.", "Настройки должны быть объектом.", "設定はオブジェクトである必要があります。", "设置必须是一个对象。", "La configuración debe ser un objeto.", "सेटिंग्स एक ऑब्जेक्ट होनी चाहिए।"),
    ("Monitor {screen} não está ativo.", "Monitor {screen} is not active.", "Bildschirm {screen} ist nicht aktiv.", "Монитор {screen} неактивен.", "モニター {screen} はアクティブではありません。", "显示器 {screen} 未启用。", "El monitor {screen} no está activo.", "मॉनिटर {screen} सक्रिय नहीं है।"),
    ("Solicitação grande demais ou incompleta.", "Request too large or incomplete.", "Anfrage zu groß oder unvollständig.", "Запрос слишком велик или неполон.", "リクエストが大きすぎるか不完全です。", "请求过大或不完整。", "Solicitud demasiado grande o incompleta.", "अनुरोध बहुत बड़ा या अधूरा है।"),
    ("Socket ocupado por outro arquivo: {path}", "Socket path occupied by another file: {path}", "Socket-Pfad durch eine andere Datei belegt: {path}", "Путь сокета занят другим файлом: {path}", "ソケットのパスは別のファイルで使用中です: {path}", "套接字路径被其他文件占用：{path}", "La ruta del socket está ocupada por otro archivo: {path}", "सॉकेट पथ पर दूसरी फ़ाइल है: {path}"),
    ("O serviço de wallpapers já está em execução.", "The wallpaper service is already running.", "Der Wallpaper-Dienst läuft bereits.", "Служба обоев уже запущена.", "壁紙サービスは既に実行中です。", "壁纸服务已在运行。", "El servicio de fondos ya está en ejecución.", "वॉलपेपर सेवा पहले से चल रही है।"),
    ("O ID do wallpaper deve ser numérico.", "The wallpaper ID must be numeric.", "Die Wallpaper-ID muss numerisch sein.", "ID обоев должен быть числом.", "壁紙 ID は数字で指定してください。", "壁纸 ID 必须是数字。", "El ID del fondo debe ser numérico.", "वॉलपेपर ID अंकों में होना चाहिए।"),
    ("Nome de monitor inválido.", "Invalid monitor name.", "Ungültiger Bildschirmname.", "Недопустимое имя монитора.", "モニター名が無効です。", "显示器名称无效。", "Nombre de monitor no válido.", "मॉनिटर का नाम अमान्य है।"),
    ("Nome de playlist inválido.", "Invalid playlist name.", "Ungültiger Wiedergabelistenname.", "Недопустимое название плейлиста.", "プレイリスト名が無効です。", "播放列表名称无效。", "Nombre de lista no válido.", "प्लेलिस्ट का नाम अमान्य है।"),
    ("O nome da playlist não pode conter caracteres de controle.", "The playlist name cannot contain control characters.", "Der Wiedergabelistenname darf keine Steuerzeichen enthalten.", "Название плейлиста не может содержать управляющие символы.", "プレイリスト名に制御文字は使えません。", "播放列表名称不能包含控制字符。", "El nombre de la lista no puede contener caracteres de control.", "प्लेलिस्ट के नाम में नियंत्रण वर्ण नहीं हो सकते।"),
    ("O nome da playlist deve ter de 1 a 80 caracteres.", "The playlist name must be 1 to 80 characters long.", "Der Wiedergabelistenname muss 1 bis 80 Zeichen lang sein.", "Название плейлиста должно содержать от 1 до 80 символов.", "プレイリスト名は 1～80 文字にしてください。", "播放列表名称必须为 1 至 80 个字符。", "El nombre de la lista debe tener entre 1 y 80 caracteres.", "प्लेलिस्ट का नाम 1 से 80 अक्षरों का होना चाहिए।"),
    ("Idioma inválido.", "Invalid language.", "Ungültige Sprache.", "Недопустимый язык.", "言語が無効です。", "语言无效。", "Idioma no válido.", "अमान्य भाषा।"),
    ("A configuração deve ser um objeto JSON.", "Configuration must be a JSON object.", "Die Konfiguration muss ein JSON-Objekt sein.", "Конфигурация должна быть объектом JSON.", "設定は JSON オブジェクトである必要があります。", "配置必须是 JSON 对象。", "La configuración debe ser un objeto JSON.", "कॉन्फ़िगरेशन एक JSON ऑब्जेक्ट होना चाहिए।"),
    ("Opções desconhecidas: {options}.", "Unknown options: {options}.", "Unbekannte Optionen: {options}.", "Неизвестные параметры: {options}.", "不明なオプション: {options}。", "未知选项：{options}。", "Opciones desconocidas: {options}.", "अज्ञात विकल्प: {options}।"),
    ("{key} deve ser verdadeiro ou falso.", "{key} must be true or false.", "{key} muss wahr oder falsch sein.", "{key} должно быть true или false.", "{key} は true または false にしてください。", "{key} 必须为 true 或 false。", "{key} debe ser verdadero o falso.", "{key} true या false होना चाहिए।"),
    ("O intervalo deve estar entre 1 e 1440 minutos.", "The interval must be between 1 and 1440 minutes.", "Das Intervall muss zwischen 1 und 1440 Minuten liegen.", "Интервал должен составлять от 1 до 1440 минут.", "間隔は 1～1440 分にしてください。", "间隔必须在 1 到 1440 分钟之间。", "El intervalo debe estar entre 1 y 1440 minutos.", "अंतराल 1 से 1440 मिनट के बीच होना चाहिए।"),
    ("FPS deve estar entre 1 e 240.", "FPS must be between 1 and 240.", "FPS muss zwischen 1 und 240 liegen.", "FPS должен быть от 1 до 240.", "FPS は 1～240 にしてください。", "FPS 必须在 1 到 240 之间。", "Los FPS deben estar entre 1 y 240.", "FPS 1 से 240 के बीच होना चाहिए।"),
    ("Escala inválida.", "Invalid scaling mode.", "Ungültiger Skalierungsmodus.", "Недопустимый режим масштабирования.", "拡大縮小モードが無効です。", "缩放模式无效。", "Modo de escala no válido.", "अमान्य स्केलिंग मोड।"),
    ("Favoritos deve ser uma lista de IDs.", "Favorites must be a list of IDs.", "Favoriten müssen eine Liste von IDs sein.", "Избранное должно быть списком ID.", "お気に入りは ID のリストにしてください。", "收藏必须是 ID 列表。", "Favoritos debe ser una lista de ID.", "पसंदीदा ID की सूची होनी चाहिए।"),
    ("Playlists deve ser um objeto de nomes e listas de IDs.", "Playlists must be an object mapping names to ID lists.", "Wiedergabelisten müssen Namen auf ID-Listen abbilden.", "Плейлисты должны быть объектом с названиями и списками ID.", "プレイリストは名前と ID リストの対応表にしてください。", "播放列表必须是名称到 ID 列表的映射对象。", "Las listas deben ser un objeto de nombres y listas de ID.", "प्लेलिस्ट नामों से ID सूचियों का ऑब्जेक्ट होना चाहिए।"),
    ("Nome de playlist duplicado: {name}.", "Duplicate playlist name: {name}.", "Doppelter Wiedergabelistenname: {name}.", "Повторное название плейлиста: {name}.", "プレイリスト名が重複しています: {name}。", "播放列表名称重复：{name}。", "Nombre de lista duplicado: {name}.", "प्लेलिस्ट का नाम दोहराया गया है: {name}।"),
    ("A playlist {name} deve conter uma lista de IDs.", "Playlist {name} must contain a list of IDs.", "Die Wiedergabeliste {name} muss eine Liste von IDs enthalten.", "Плейлист {name} должен содержать список ID.", "プレイリスト {name} には ID のリストが必要です。", "播放列表 {name} 必须包含 ID 列表。", "La lista {name} debe contener una lista de ID.", "प्लेलिस्ट {name} में ID की सूची होनी चाहिए।"),
    ("Playlist ativa não encontrada: {name}.", "Active playlist not found: {name}.", "Aktive Wiedergabeliste nicht gefunden: {name}.", "Активный плейлист не найден: {name}.", "アクティブなプレイリストが見つかりません: {name}。", "找不到当前播放列表：{name}。", "No se encontró la lista activa: {name}.", "सक्रिय प्लेलिस्ट नहीं मिली: {name}।"),
    ("Atribuições de monitor devem ser um objeto.", "Monitor assignments must be an object.", "Bildschirmzuweisungen müssen ein Objekt sein.", "Закрепления за мониторами должны быть объектом.", "モニター割り当てはオブジェクトにしてください。", "显示器分配必须是一个对象。", "Las asignaciones de monitor deben ser un objeto.", "मॉनिटर असाइनमेंट एक ऑब्जेक्ट होने चाहिए।"),
    ("Caminho do renderizador inválido.", "Invalid renderer path.", "Ungültiger Renderer-Pfad.", "Недопустимый путь к рендереру.", "レンダラーのパスが無効です。", "渲染器路径无效。", "Ruta del motor no válida.", "रेंडरर पथ अमान्य है।"),
    ("O caminho do renderizador deve ser absoluto ou 'auto'.", "The renderer path must be absolute or 'auto'.", "Der Renderer-Pfad muss absolut oder 'auto' sein.", "Путь к рендереру должен быть абсолютным или 'auto'.", "レンダラーのパスは絶対パスか 'auto' にしてください。", "渲染器路径必须是绝对路径或 'auto'。", "La ruta del motor debe ser absoluta o 'auto'.", "रेंडरर पथ पूर्ण पथ या 'auto' होना चाहिए।"),
    ("Estado inválido", "Invalid state", "Ungültiger Status", "Недопустимое состояние", "無効な状態", "无效状态", "Estado no válido", "अमान्य स्थिति"),
    ("Idioma da interface", "Interface language", "Sprache der Oberfläche", "Язык интерфейса", "表示言語", "界面语言", "Idioma de la interfaz", "इंटरफ़ेस की भाषा"),
    ("IDIOMA DA INTERFACE", "INTERFACE LANGUAGE", "SPRACHE DER OBERFLÄCHE", "ЯЗЫК ИНТЕРФЕЙСА", "表示言語", "界面语言", "IDIOMA DE LA INTERFAZ", "इंटरफ़ेस की भाषा"),
    ("Aplica a tradução imediatamente.", "Applies the translation immediately.", "Wendet die Übersetzung sofort an.", "Применяет перевод сразу.", "翻訳をすぐに適用します。", "立即应用翻译。", "Aplica la traducción inmediatamente.", "अनुवाद तुरंत लागू करता है।"),
    ("{count} itens · {state}", "{count} items · {state}", "{count} Elemente · {state}", "Элементов: {count} · {state}", "{count} 件 · {state}", "{count} 项 · {state}", "{count} elementos · {state}", "{count} आइटम · {state}"),
    ("PLAYLISTS", "PLAYLISTS", "WIEDERGABELISTEN", "ПЛЕЙЛИСТЫ", "プレイリスト", "播放列表", "LISTAS DE REPRODUCCIÓN", "प्लेलिस्ट"),
    ("Automático (sistema)", "Automatic (system)", "Automatisch (System)", "Автоматически (система)", "自動 (システム)", "自动（系统）", "Automático (sistema)", "स्वचालित (सिस्टम)"),
    ("Solicitação grande demais.", "Request is too large.", "Anfrage ist zu groß.", "Запрос слишком велик.", "リクエストが大きすぎます。", "请求过大。", "La solicitud es demasiado grande.", "अनुरोध बहुत बड़ा है।"),
    ("Serviço de wallpapers indisponível. Inicie o serviço e tente novamente.", "Wallpaper service unavailable. Start the service and try again.", "Wallpaper-Dienst nicht verfügbar. Starte den Dienst und versuche es erneut.", "Служба обоев недоступна. Запустите службу и повторите попытку.", "壁紙サービスを利用できません。サービスを起動して再試行してください。", "壁纸服务不可用。请启动服务后重试。", "El servicio de fondos no está disponible. Inícialo e inténtalo de nuevo.", "वॉलपेपर सेवा उपलब्ध नहीं है। सेवा शुरू करके फिर से प्रयास करें।"),
    ("Resposta inválida do serviço de wallpapers.", "Invalid response from the wallpaper service.", "Ungültige Antwort vom Wallpaper-Dienst.", "Некорректный ответ службы обоев.", "壁紙サービスからの応答が無効です。", "壁纸服务的响应无效。", "Respuesta no válida del servicio de fondos.", "वॉलपेपर सेवा से अमान्य जवाब मिला।"),
    ("O serviço recusou a solicitação.", "The service rejected the request.", "Der Dienst hat die Anfrage abgelehnt.", "Служба отклонила запрос.", "サービスがリクエストを拒否しました。", "服务拒绝了请求。", "El servicio rechazó la solicitud.", "सेवा ने अनुरोध अस्वीकार कर दिया।"),
    ("Personalizar tema", "Customize theme", "Design anpassen", "Настроить тему", "テーマをカスタマイズ", "自定义主题", "Personalizar tema", "थीम को अनुकूलित करें"),
    ("Tema", "Theme", "Design", "Тема", "テーマ", "主题", "Tema", "थीम"),
    ("Redefinir", "Reset", "Zurücksetzen", "Сбросить", "リセット", "重置", "Restablecer", "रीसेट करें"),
    ("Matiz", "Hue", "Farbton", "Оттенок", "色相", "色相", "Tono", "रंगत"),
    ("Intensidade", "Intensity", "Intensität", "Интенсивность", "強さ", "强度", "Intensidad", "तीव्रता"),
    ("Predefinições", "Presets", "Voreinstellungen", "Готовые варианты", "プリセット", "预设", "Preajustes", "प्रीसेट"),
    ("Âmbar", "Amber", "Bernstein", "Янтарный", "アンバー", "琥珀色", "Ámbar", "अंबर"),
    ("Dourado", "Gold", "Gold", "Золотой", "ゴールド", "金色", "Dorado", "सुनहरा"),
    ("Vermelho", "Red", "Rot", "Красный", "赤", "红色", "Rojo", "लाल"),
    ("Violeta", "Violet", "Violett", "Фиолетовый", "バイオレット", "紫色", "Violeta", "बैंगनी"),
    ("Azul", "Blue", "Blau", "Синий", "青", "蓝色", "Azul", "नीला"),
    ("Verde", "Green", "Grün", "Зелёный", "緑", "绿色", "Verde", "हरा"),
    ("Não foi possível salvar o tema: {error}", "Could not save the theme: {error}", "Das Design konnte nicht gespeichert werden: {error}", "Не удалось сохранить тему: {error}", "テーマを保存できませんでした: {error}", "无法保存主题：{error}", "No se pudo guardar el tema: {error}", "थीम सहेजी नहीं जा सकी: {error}"),
    ("Matiz da interface deve estar entre 0 e 360.", "Interface hue must be between 0 and 360.", "Der Farbton der Oberfläche muss zwischen 0 und 360 liegen.", "Оттенок интерфейса должен быть от 0 до 360.", "画面の色相は 0～360 にしてください。", "界面色相必须在 0 到 360 之间。", "El tono de la interfaz debe estar entre 0 y 360.", "इंटरफ़ेस की रंगत 0 से 360 के बीच होनी चाहिए।"),
    ("Intensidade da interface deve estar entre 35 e 100.", "Interface intensity must be between 35 and 100.", "Die Intensität der Oberfläche muss zwischen 35 und 100 liegen.", "Интенсивность интерфейса должна быть от 35 до 100.", "画面の強さは 35～100 にしてください。", "界面强度必须在 35 到 100 之间。", "La intensidad de la interfaz debe estar entre 35 y 100.", "इंटरफ़ेस की तीव्रता 35 से 100 के बीच होनी चाहिए।"),
    ("Ativa o serviço do usuário quando a sessão gráfica é iniciada.", "Starts the user service when the graphical session begins.", "Startet den Benutzerdienst beim Beginn der grafischen Sitzung.", "Запускает пользовательскую службу при запуске графического сеанса.", "グラフィカルセッションの開始時にユーザーサービスを起動します。", "图形会话启动时启动用户服务。", "Inicia el servicio de usuario al comenzar la sesión gráfica.", "ग्राफ़िकल सत्र शुरू होने पर उपयोगकर्ता सेवा चालू करता है।"),
    ("Ativar o serviço com a sessão", "Start the service with the session", "Den Dienst mit der Sitzung starten", "Запускать службу при входе", "セッション開始時にサービスを起動", "登录时启动服务", "Iniciar el servicio con la sesión", "सेशन के साथ सेवा शुरू करें"),
    ("Minimizar para a bandeja ao fechar", "Minimize to the tray on close", "Beim Schließen in den Infobereich minimieren", "Сворачивать в трей при закрытии", "閉じるとトレイに最小化", "关闭时最小化到托盘", "Minimizar a la bandeja al cerrar", "बंद करने पर ट्रे में छोटा करें"),
    ("Mantém o aplicativo aberto na bandeja; use o menu do ícone para reabrir ou sair.", "Keeps the app running in the tray; use the icon menu to reopen or quit.", "Lässt die App im Infobereich weiterlaufen; über das Symbolmenü kannst du sie öffnen oder beenden.", "Оставляет приложение в трее; откройте его или завершите через меню значка.", "アプリをトレイで実行し続けます。アイコンのメニューから再表示または終了できます。", "应用将在托盘中继续运行；可通过图标菜单重新打开或退出。", "Mantiene la aplicación en la bandeja; usa el menú del icono para abrirla o salir.", "ऐप ट्रे में चलता रहेगा; आइकन मेनू से फिर खोलें या बाहर निकलें।"),
    ("Iniciar o aplicativo com o sistema", "Start the app with the system", "Die App mit dem System starten", "Запускать приложение при входе в систему", "システム起動時にアプリを起動", "随系统启动应用", "Iniciar la aplicación con el sistema", "सिस्टम के साथ ऐप शुरू करें"),
    ("Abre o painel quando você entra na sessão gráfica.", "Opens the control panel when you sign in to your desktop session.", "Öffnet das Bedienfeld, sobald du dich an deiner grafischen Sitzung anmeldest.", "Открывает панель управления при входе в графический сеанс.", "グラフィカルセッションへのログイン時に操作パネルを開きます。", "登录图形会话时打开控制面板。", "Abre el panel al iniciar tu sesión gráfica.", "डेस्कटॉप सत्र में लॉग इन करते ही कंट्रोल पैनल खोलता है।"),
    ("Restaurar a janela", "Show window", "Fenster anzeigen", "Показать окно", "ウィンドウを表示", "显示窗口", "Mostrar ventana", "विंडो दिखाएँ"),
    ("Sair do aplicativo", "Quit application", "Anwendung beenden", "Выйти из приложения", "アプリを終了", "退出应用", "Salir de la aplicación", "ऐप से बाहर निकलें"),
    ("Não foi possível iniciar a bandeja: {error}", "Could not start the tray: {error}", "Der Infobereich konnte nicht gestartet werden: {error}", "Не удалось запустить трей: {error}", "トレイを起動できませんでした: {error}", "无法启动托盘：{error}", "No se pudo iniciar la bandeja: {error}", "ट्रे शुरू नहीं हो सकी: {error}"),
    ("Não foi possível alterar a preferência: {error}", "Could not change the preference: {error}", "Die Einstellung konnte nicht geändert werden: {error}", "Не удалось изменить настройку: {error}", "設定を変更できませんでした: {error}", "无法更改偏好设置：{error}", "No se pudo cambiar la preferencia: {error}", "पसंद नहीं बदली जा सकी: {error}"),
    ("Não foi possível salvar a preferência: {error}", "Could not save the preference: {error}", "Die Einstellung konnte nicht gespeichert werden: {error}", "Не удалось сохранить настройку: {error}", "設定を保存できませんでした: {error}", "无法保存偏好设置：{error}", "No se pudo guardar la preferencia: {error}", "पसंद सहेजी नहीं जा सकी: {error}"),
    ("A bandeja requer GTK 3 e Ayatana AppIndicator.", "The tray requires GTK 3 and Ayatana AppIndicator.", "Der Infobereich benötigt GTK 3 und Ayatana AppIndicator.", "Для трея нужны GTK 3 и Ayatana AppIndicator.", "トレイには GTK 3 と Ayatana AppIndicator が必要です。", "托盘功能需要 GTK 3 和 Ayatana AppIndicator。", "La bandeja requiere GTK 3 y Ayatana AppIndicator.", "ट्रे के लिए GTK 3 और Ayatana AppIndicator ज़रूरी हैं।"),
    ("A bandeja precisa do Ayatana AppIndicator ou do AppIndicator3. Instale: Debian/Ubuntu gir1.2-ayatanaappindicator3-0.1, Fedora libayatana-appindicator-gtk3, Arch libayatana-appindicator.", "The tray needs Ayatana AppIndicator or AppIndicator3. Install: Debian/Ubuntu gir1.2-ayatanaappindicator3-0.1, Fedora libayatana-appindicator-gtk3, Arch libayatana-appindicator.", "Der Infobereich benötigt Ayatana AppIndicator oder AppIndicator3. Installiere: Debian/Ubuntu gir1.2-ayatanaappindicator3-0.1, Fedora libayatana-appindicator-gtk3, Arch libayatana-appindicator.", "Для трея нужны Ayatana AppIndicator или AppIndicator3. Установите: Debian/Ubuntu gir1.2-ayatanaappindicator3-0.1, Fedora libayatana-appindicator-gtk3, Arch libayatana-appindicator.", "トレイには Ayatana AppIndicator または AppIndicator3 が必要です。パッケージをインストールしてください: Debian/Ubuntu gir1.2-ayatanaappindicator3-0.1、Fedora libayatana-appindicator-gtk3、Arch libayatana-appindicator。", "托盘需要 Ayatana AppIndicator 或 AppIndicator3。请安装：Debian/Ubuntu gir1.2-ayatanaappindicator3-0.1、Fedora libayatana-appindicator-gtk3、Arch libayatana-appindicator。", "La bandeja necesita Ayatana AppIndicator o AppIndicator3. Instala: Debian/Ubuntu gir1.2-ayatanaappindicator3-0.1, Fedora libayatana-appindicator-gtk3, Arch libayatana-appindicator.", "ट्रे के लिए Ayatana AppIndicator या AppIndicator3 ज़रूरी है। इंस्टॉल करें: Debian/Ubuntu gir1.2-ayatanaappindicator3-0.1, Fedora libayatana-appindicator-gtk3, Arch libayatana-appindicator।"),
    ("Opções desconhecidas para o wallpaper {wallpaper_id}: {names}.", "Unknown options for wallpaper {wallpaper_id}: {names}.", "Unbekannte Optionen für Wallpaper {wallpaper_id}: {names}.", "Неизвестные параметры для обоев {wallpaper_id}: {names}.", "壁紙 {wallpaper_id} の不明なオプション: {names}。", "壁纸 {wallpaper_id} 的未知选项：{names}。", "Opciones desconocidas para el fondo {wallpaper_id}: {names}.", "वॉलपेपर {wallpaper_id} के लिए अज्ञात विकल्प: {names}।"),
    ("As opções dos wallpapers devem ser um objeto.", "Wallpaper options must be an object.", "Wallpaper-Optionen müssen ein Objekt sein.", "Параметры обоев должны быть объектом.", "壁紙のオプションはオブジェクトにしてください。", "壁纸选项必须是一个对象。", "Las opciones de los fondos deben ser un objeto.", "वॉलपेपर विकल्प एक ऑब्जेक्ट होने चाहिए।"),
    ("As opções de cada wallpaper devem ser um objeto válido.", "Each wallpaper's options must be a valid object.", "Die Optionen jedes Wallpapers müssen ein gültiges Objekt sein.", "Параметры каждых обоев должны быть корректным объектом.", "各壁紙のオプションは有効なオブジェクトにしてください。", "每个壁纸的选项都必须是有效对象。", "Las opciones de cada fondo deben ser un objeto válido.", "हर वॉलपेपर के विकल्प एक मान्य ऑब्जेक्ट होने चाहिए।"),
    ("Nome de opção de wallpaper inválido.", "Invalid wallpaper option name.", "Ungültiger Name für Wallpaper-Option.", "Недопустимое имя параметра обоев.", "壁紙オプション名が無効です。", "壁纸选项名称无效。", "Nombre de opción de fondo no válido.", "वॉलपेपर विकल्प का नाम अमान्य है।"),
    ("As opções de wallpaper devem ser verdadeiras ou falsas.", "Wallpaper options must be true or false.", "Wallpaper-Optionen müssen wahr oder falsch sein.", "Параметры обоев должны быть true или false.", "壁紙のオプションは true または false にしてください。", "壁纸选项必须为 true 或 false。", "Las opciones del fondo deben ser verdaderas o falsas.", "वॉलपेपर विकल्प true या false होने चाहिए।"),
    ("As preferências da interface devem ser um objeto.", "Interface preferences must be an object.", "Oberflächeneinstellungen müssen ein Objekt sein.", "Настройки интерфейса должны быть объектом.", "インターフェース設定はオブジェクトにしてください。", "界面偏好设置必须是一个对象。", "Las preferencias de la interfaz deben ser un objeto.", "इंटरफ़ेस प्राथमिकताएँ एक ऑब्जेक्ट होनी चाहिए।"),
    ("Preferência de aparência inválida: {key}.", "Invalid appearance preference: {key}.", "Ungültige Darstellungseinstellung: {key}.", "Недопустимая настройка оформления: {key}.", "外観の設定が無効です: {key}。", "外观偏好设置无效：{key}。", "Preferencia de apariencia no válida: {key}.", "अमान्य दिखावट प्राथमिकता: {key}।"),
    ("Preferência da interface inválida: {key}.", "Invalid interface preference: {key}.", "Ungültige Oberflächeneinstellung: {key}.", "Недопустимая настройка интерфейса: {key}.", "インターフェース設定が無効です: {key}。", "界面偏好设置无效：{key}。", "Preferencia de la interfaz no válida: {key}.", "अमान्य इंटरफ़ेस प्राथमिकता: {key}।"),
    ("A opção de inicialização deve ser verdadeira ou falsa.", "The startup option must be true or false.", "Die Startoption muss wahr oder falsch sein.", "Параметр автозапуска должен быть true или false.", "起動オプションは true または false にしてください。", "启动选项必须为 true 或 false。", "La opción de inicio debe ser verdadera o falsa.", "स्टार्टअप विकल्प true या false होना चाहिए।"),
    ("Não foi possível localizar o inicializador do aplicativo.", "Could not find the application launcher.", "Das Startprogramm der Anwendung wurde nicht gefunden.", "Не удалось найти средство запуска приложения.", "アプリのランチャーが見つかりませんでした。", "找不到应用程序启动器。", "No se pudo encontrar el lanzador de la aplicación.", "ऐप लॉन्चर नहीं मिला।"),
    ("O caminho do inicializador contém caracteres inválidos.", "The launcher path contains invalid characters.", "Der Pfad des Startprogramms enthält ungültige Zeichen.", "Путь к средству запуска содержит недопустимые символы.", "ランチャーのパスに無効な文字が含まれています。", "启动器路径包含无效字符。", "La ruta del lanzador contiene caracteres no válidos.", "लॉन्चर पथ में अमान्य वर्ण हैं।"),
    ("iniciar", "start", "gestartet", "запуск", "開始", "启动", "iniciar", "प्रारंभ"),
    ("ativar", "enable", "aktiviert", "включение", "有効化", "启用", "activar", "सक्रिय"),
    ("desativar", "disable", "deaktiviert", "отключение", "無効化", "禁用", "desactivar", "निष्क्रिय"),
)


_CATALOG = {
    code: {row[0]: row[index] for row in _ROWS}
    for index, code in enumerate(_CODES)
    if code != "pt_BR"
}


def _match_language(value: str | None) -> str | None:
    if not value:
        return None
    normalized = value.split(".", 1)[0].split("@", 1)[0].replace("-", "_").lower()
    if normalized in {"c", "posix"}:
        return "en"
    language = normalized.split("_", 1)[0]
    if language == "pt":
        return "pt_BR"
    if language == "zh":
        return "zh_CN"
    if language in {"en", "de", "ru", "ja", "es", "hi"}:
        return language
    return None


def system_language() -> str:
    """Choose a supported language using the desktop's locale preferences."""
    # LANGUAGE expresses a UI translation preference, including a fallback list.
    for candidate in os.environ.get("LANGUAGE", "").split(":"):
        matched = _match_language(candidate)
        if matched:
            return matched

    # Launchers and terminals can pass a C.UTF-8 locale even when the Plasma
    # session has a different language. Follow the user's Plasma setting here.
    if "KDE" in os.environ.get("XDG_CURRENT_DESKTOP", "").upper().split(":"):
        config_home = Path(os.environ.get("XDG_CONFIG_HOME") or Path.home() / ".config")
        plasma_locale = configparser.ConfigParser(interpolation=None)
        try:
            plasma_locale.read(config_home / "plasma-localerc", encoding="utf-8")
        except (OSError, UnicodeError, configparser.Error):
            pass
        else:
            for section, option in (("Translations", "LANGUAGE"), ("Formats", "LANG")):
                value = plasma_locale.get(section, option, fallback="")
                for candidate in value.split(":"):
                    matched = _match_language(candidate)
                    if matched:
                        return matched

    for variable in ("LC_ALL", "LC_MESSAGES", "LANG"):
        for candidate in os.environ.get(variable, "").split(":"):
            matched = _match_language(candidate)
            if matched:
                return matched
    try:
        selected = locale.getlocale(locale.LC_MESSAGES)[0]
    except (ValueError, AttributeError):
        selected = None
    return _match_language(selected) or "en"


_language = system_language()


def set_language(code: str) -> str:
    """Select a language for this process; return the effective language."""
    global _language
    if code == "auto":
        _language = system_language()
        return _language
    selected = _match_language(code)
    if selected is None or selected not in _CODES:
        raise ValueError(f"Unsupported language: {code}")
    _language = selected
    return _language


def current_language() -> str:
    return _language


def language_options() -> list[tuple[str, str]]:
    """Return codes and native display names for a language selector."""
    return list(_LANGUAGES)


def tr(source: str, **kwargs: object) -> str:
    """Translate a Portuguese source message and fill named placeholders."""
    translated = _CATALOG.get(_language, {}).get(source, source)
    return translated.format(**kwargs) if kwargs else translated
