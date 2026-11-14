// ---- BẢO MẬT: phiên đăng nhập do server cấp (cookie HttpOnly). Khi server trả 401 thì dọn trạng thái cục bộ và về trang đăng nhập.
(function () {
  const _fetch = window.fetch.bind(window);
  window.fetch = async function (input, init) {
    const res = await _fetch(input, init);
    try {
      const url = typeof input === 'string' ? input : (input && input.url) || '';
      const path = url.replace(/^https?:\/\/[^/]+/, '');
      if (res.status === 401 && path.startsWith('/api/') && !path.startsWith('/api/auth/')) {
        localStorage.removeItem('cs_user');
        localStorage.removeItem('cs_admin_token');
        if (!window.__authRedirecting) {
          window.__authRedirecting = true;
          sessionStorage.setItem('cs_logged_out', '1');
          window.location.href = 'index.html';
        }
      }
    } catch (_) {}
    return res;
  };
})();

/**
 * Mạng Xã Hội MIni - C++ Web Platform & SQLite Engine Client
 * Bao gồm đầy đủ tính năng: Profile riêng, Modal bình luận, Nhắn tin nhóm,
 * Chia sẻ vào chat, Gợi ý tìm kiếm thời gian thực, Bộ lọc bạn bè, Thông báo.
 */

const CHAT_EMOJI_CATEGORIES = {
  smileys: { label: 'Mặt cười và người', icon: '😄', emojis: '😀 😃 😄 😁 😆 😅 😂 🤣 🥲 ☺️ 😊 😇 🙂 🙃 😉 😌 😍 🥰 😘 😗 😙 😚 😋 😛 😝 😜 🤪 🤨 🧐 🤓 😎 🥸 🤩 🥳 😏 😒 😞 😔 😟 😕 🙁 ☹️ 😣 😖 😫 😩 🥺 😢 😭 😤 😠 😡 🤬 🤯 😳 🥵 🥶 😱 😨 😰 😥 😓 🤗 🤔 🫣 🤭 🫢 🫡 🤫 🫠 🤥 😶 😶‍🌫️ 😐 😑 😬 🫨 🫥 😯 😦 😧 😮 😲 🥱 😴 🤤 😪 😵 😵‍💫 🤐 🥴 🤢 🤮 🤧 😷 🤒 🤕 🤑 🤠 😈 👿 👹 👺 🤡 💩 👻 💀 ☠️ 👽 👾 🤖 🎃 😺 😸 😹 😻 😼 😽 🙀 😿 😾 👋 🤚 🖐️ ✋ 🖖 🫱 🫲 🫳 🫴 👌 🤌 🤏 ✌️ 🤞 🫰 🤟 🤘 🤙 👈 👉 👆 🖕 👇 ☝️ 🫵 👍 👎 ✊ 👊 🤛 🤜 👏 🙌 🫶 👐 🤲 🤝 🙏 ✍️ 💅 🤳 💪 🦾 🦿 🦵 🦶 👂 🦻 👃 🧠 🫀 🫁 🦷 🦴 👀 👁️ 👅 👄 🫦 👶 🧒 👦 👧 🧑 👨 👩 🧔 👱 🧓 👴 👵 🙍 🙎 🙅 🙆 💁 🙋 🧏 🙇 🤦 🤷 👮 🕵️ 💂 🥷 👷 🫅 🤴 👸 👳 👲 🧕 🤵 👰 🤰 🫃 🫄 🤱 👼 🎅 🤶 🧑‍🎄 🦸 🦹 🧙 🧚 🧛 🧜 🧝 🧞 🧟 💆 💇 🚶 🧍 🧎 🏃 💃 🕺 🕴️ 👯 🧖 🧗 🤸 🏌️ 🧘 🛀 🛌 👭 👫 👬 💏 💑 👪 🗣️ 👤 👥 🫂'},
  animals: { label: 'Động vật và thiên nhiên', icon: '🐻', emojis: '🐶 🐱 🐭 🐹 🐰 🦊 🐻 🐼 🐻‍❄️ 🐨 🐯 🦁 🐮 🐷 🐽 🐸 🐵 🙈 🙉 🙊 🐒 🦍 🦧 🐔 🐧 🐦 🐦‍⬛ 🐤 🐣 🐥 🦆 🦅 🦉 🦇 🐺 🐗 🐴 🦄 🫎 🫏 🐝 🪱 🐛 🦋 🐌 🐞 🐜 🪰 🪲 🪳 🦟 🦗 🕷️ 🕸️ 🦂 🐢 🐍 🦎 🦖 🦕 🐙 🦑 🦐 🦞 🦀 🪼 🪸 🐠 🐟 🐡 🐬 🦈 🐳 🐋 🦭 🐊 🐅 🐆 🦓 🦍 🦣 🐘 🦛 🦏 🐪 🐫 🦒 🦘 🦬 🐃 🐂 🐄 🐎 🐖 🐏 🐑 🦙 🐐 🦌 🐕 🐩 🐈 🐓 🦃 🦤 🦚 🦜 🪽 🪿 🦢 🦩 🕊️ 🐇 🦝 🦨 🦡 🦫 🦦 🦥 🐁 🐀 🐿️ 🦔 🐾 🐉 🐲 🌵 🎄 🌲 🌳 🌴 🪵 🌱 🌿 ☘️ 🍀 🎍 🪴 🎋 🍃 🍂 🍁 🍄 🪺 🪹 🌾 💐 🌷 🪷 🌹 🥀 🌺 🌸 🌼 🌻 🌞 🌝 🌛 🌜 🌚 🌕 🌖 🌗 🌘 🌑 🌒 🌓 🌔 🌙 🌎 🌍 🌏 🪐 💫 ⭐ 🌟 ✨ ⚡ ☄️ 💥 🔥 🌪️ 🌈 ☀️ 🌤️ ⛅ 🌥️ ☁️ 🌦️ 🌧️ ⛈️ 🌩️ 🌨️ ❄️ ☃️ ⛄ 🌬️ 💨 💧 💦 ☔ ☂️ 🌊'},
  food: { label: 'Đồ ăn và thức uống', icon: '🍔', emojis: '🍇 🍈 🍉 🍊 🍋 🍋‍🟩 🍌 🍍 🥭 🍎 🍏 🍐 🍑 🍒 🍓 🫐 🥝 🍅 🫒 🥥 🥑 🍆 🥔 🥕 🌽 🌶️ 🫑 🥒 🥬 🥦 🧄 🧅 🍄 🥜 🫘 🌰 🍞 🥐 🥖 🫓 🥨 🥯 🥞 🧇 🧀 🍖 🍗 🥩 🥓 🍔 🍟 🍕 🌭 🥪 🌮 🌯 🫔 🥙 🧆 🥚 🍳 🥘 🍲 🫕 🥣 🥗 🍿 🧈 🧂 🥫 🍱 🍘 🍙 🍚 🍛 🍜 🍝 🍠 🍢 🍣 🍤 🍥 🥮 🍡 🥟 🥠 🥡 🦪 🍦 🍧 🍨 🍩 🍪 🎂 🍰 🧁 🥧 🍫 🍬 🍭 🍮 🍯 🍼 🥛 ☕ 🫖 🍵 🍶 🍾 🍷 🍸 🍹 🍺 🍻 🥂 🥃 🫗 🥤 🧋 🧃 🧉 🧊 🥢 🍽️ 🍴 🥄'},
  activities: { label: 'Hoạt động và thể thao', icon: '⚽', emojis: '⚽ 🏀 🏈 ⚾ 🥎 🎾 🏐 🏉 🥏 🎱 🪀 🏓 🏸 🏒 🏑 🥍 🏏 🪃 🥅 ⛳ 🪁 🏹 🎣 🤿 🥊 🥋 🎽 🛹 🛼 🛷 ⛸️ 🥌 🎿 ⛷️ 🏂 🏋️ 🤼 🤸 ⛹️ 🤺 🏌️ 🏇 🧘 🏄 🏊 🤽 🚣 🧗 🚵 🚴 🏆 🥇 🥈 🥉 🏅 🎖️ 🏵️ 🎗️ 🎫 🎟️ 🎪 🤹 🎭 🩰 🎨 🎬 🎤 🎧 🎼 🎹 🥁 🪘 🎷 🎺 🪗 🎸 🪕 🎻 🪈 🎲 ♟️ 🎯 🎳 🎮 🕹️ 🎰 🧩'},
  travel: { label: 'Du lịch và địa điểm', icon: '🚗', emojis: '🚗 🚕 🚙 🚌 🚎 🏎️ 🚓 🚑 🚒 🚐 🛻 🚚 🚛 🚜 🦯 🦽 🦼 🛴 🚲 🛵 🏍️ 🛺 🚨 🚔 🚍 🚘 🚖 🚡 🚠 🚟 🚃 🚋 🚞 🚝 🚄 🚅 🚈 🚂 🚆 🚇 🚊 🚉 ✈️ 🛫 🛬 🛩️ 💺 🛰️ 🚀 🛸 🚁 🛶 ⛵ 🚤 🛥️ 🛳️ ⛴️ 🚢 ⚓ 🛟 ⛽ 🚧 🚦 🚥 🚏 🗺️ 🗿 🗽 🗼 🏰 🏯 🏟️ 🎡 🎢 🎠 ⛲ ⛱️ 🏖️ 🏝️ 🏜️ 🌋 ⛰️ 🏔️ 🗻 🏕️ ⛺ 🛖 🏠 🏡 🏘️ 🏚️ 🏗️ 🏭 🏢 🏬 🏣 🏤 🏥 🏦 🏨 🏪 🏫 🏩 💒 🏛️ ⛪ 🕌 🕍 🛕 🕋 ⛩️ 🛤️ 🛣️ 🗾 🎑 🏞️ 🌅 🌄 🌠 🎇 🎆 🌇 🌆 🏙️ 🌃 🌌 🌉 🌁'},
  objects: { label: 'Đồ vật', icon: '💡', emojis: '⌚ 📱 📲 💻 ⌨️ 🖥️ 🖨️ 🖱️ 🖲️ 🕹️ 💽 💾 💿 📀 📼 📷 📸 📹 🎥 📽️ 🎞️ 📞 ☎️ 📟 📠 📺 📻 🎙️ 🎚️ 🎛️ 🧭 ⏱️ ⏲️ ⏰ 🕰️ ⌛ ⏳ 📡 🔋 🪫 🔌 💡 🔦 🕯️ 🪔 🧯 🛢️ 💸 💵 💴 💶 💷 🪙 💰 💳 💎 ⚖️ 🪜 🧰 🪛 🔧 🔨 ⚒️ 🛠️ ⛏️ 🪚 🔩 ⚙️ 🪤 🧱 ⛓️ 🧲 🔫 💣 🧨 🪓 🔪 🗡️ ⚔️ 🛡️ 🚬 ⚰️ 🪦 ⚱️ 🏺 🔮 📿 🧿 🪬 💈 ⚗️ 🔭 🔬 🕳️ 🩹 🩺 🩻 🩼 💊 💉 🩸 🧬 🦠 🧫 🧪 🌡️ 🧹 🪠 🧺 🧻 🚽 🚰 🚿 🛁 🛀 🧼 🪥 🪒 🧽 🪣 🧴 🛎️ 🔑 🗝️ 🚪 🪑 🛋️ 🛏️ 🛌 🧸 🪆 🖼️ 🪞 🪟 🛍️ 🛒 🎁 🎈 🎏 🎀 🪄 🪅 🎊 🎉 🎎 🏮 🎐 🧧 ✉️ 📩 📨 📧 💌 📥 📤 📦 🏷️ 📪 📫 📬 📭 📮 📯 📜 📃 📄 📑 🧾 📊 📈 📉 📇 🗒️ 🗓️ 📅 📆 📋 📁 📂 🗂️ 🗃️ 🗄️ 📌 📍 📎 🖇️ 📏 📐 ✂️ 🗑️ 🔒 🔓 🔏 🔐 🔑 🔨 💻 📝 ✏️ ✒️ 🖋️ 🖊️ 🖌️ 🖍️ 🖇️ 📚 📖 🔖 🧮 📕 📗 📘 📙'},
  symbols: { label: 'Biểu tượng', icon: '💜', emojis: '❤️ 🧡 💛 💚 💙 💜 🖤 🤍 🤎 🩷 🩵 🩶 💔 ❤️‍🔥 ❤️‍🩹 💕 💞 💓 💗 💖 💘 💝 💟 ❣️ 💌 💋 💯 💢 💥 💫 💦 💨 🕳️ 💬 👁️‍🗨️ 🗨️ 🗯️ 💭 💤 💮 ♨️ 💈 🛑 🚫 ⛔ 📛 🚷 🚯 🚳 🚱 🔞 📵 🚭 ❌ ⭕ 💢 ♻️ 🔱 📛 🔰 ✅ ☑️ ✔️ ❎ ❌ ❗ ❕ ❓ ❔ ‼️ ⁉️ 🔅 🔆 〽️ ⚠️ 🚸 🔗 🔒 🔓 🔏 🔐 🔑 🛡️ ⚔️ ⚒️ 🛠️ 🧰 🧲 ⚙️ ⚖️ 🦯 🛟 ⛑️ 🪪 🔔 🔕 🎵 🎶 🏧 🚮 🚰 ♿ 🚹 🚺 🚻 🚼 🚾 🛂 🛃 🛄 🛅 ⚧️ 🚺 🚹 🛗 🚭 🚬 🅿️ 🅰️ 🅱️ 🆎 🅾️ 🆑 🆘 🆔 🆕 🆙 🆚 🆓 🆒 🆖 🆗 🆙 🈁 🈂️ 🈷️ 🈶 🈯 🉐 🈹 🈚 🈲 🉑 🈸 🈴 🈳 🈵 🔴 🟠 🟡 🟢 🔵 🟣 🟤 ⚫ ⚪ 🟥 🟧 🟨 🟩 🟦 🟪 🟫 ⬛ ⬜ ◼️ ◻️ 🔶 🔷 🔸 🔹 🔺 🔻 💠 🔘 🔳 🔲 ⭐ 🌟 ✨ 💫 🌈 ☮️ ✝️ ☪️ 🕉️ ☸️ ✡️ 🔯 🕎 ☯️ ☦️ 🛐 ⛎ ♈ ♉ ♊ ♋ ♌ ♍ ♎ ♏ ♐ ♑ ♒ ♓ ⛔ ♀️ ♂️ ⚕️ ✖️ ➕ ➖ ➗ 🟰 ♾️ ‼️ ⁉️ ❓ ❗ 〰️ ➰ ➿ 🔃 🔄 🔙 🔚 🔛 🔜 🔝 ⬆️ ⬇️ ⬅️ ➡️ ↗️ ↘️ ↙️ ↖️ ↕️ ↔️ 🔀 🔁 🔂 🔼 🔽 ⏫ ⏬ ▶️ ⏩ ⏭️ ⏯️ ◀️ ⏪ ⏮️ 🔺 🔻 ⏸️ ⏹️ ⏺️ ⏏️ 🎦 🔅 🔆 🔇 🔈 🔉 🔊 📶 📳 📴'},
  flags: { label: 'Cờ', icon: '🏳️', emojis: '🏳️ 🏴 🏁 🚩 🏳️‍🌈 🏳️‍⚧️ 🇻🇳 🇺🇸 🇬🇧 🇯🇵 🇰🇷 🇨🇳 🇹🇭 🇸🇬 🇫🇷 🇩🇪 🇮🇹 🇪🇸 🇵🇹 🇷🇺 🇺🇦 🇨🇦 🇦🇺 🇳🇿 🇧🇷 🇲🇽 🇮🇳 🇮🇩 🇲🇾 🇵🇭 🇳🇱 🇸🇪 🇳🇴 🇫🇮 🇩🇰 🇨🇭 🇦🇷 🇨🇱 🇪🇬 🇿🇦 🇹🇷 🇵🇰 🇸🇦 🇦🇪 🇮🇪 🇬🇷 🇵🇱 🇨🇿 🇭🇰 🇹🇼 🇲🇴'}
};
const DEFAULT_CHAT_QUICK_REACTIONS = ['👍', '❤️', '😂', '😮', '😢', '🙏'];

const CHAT_EMOJI_SEARCH_NAMES = {
  '😀': 'cười vui', '😂': 'cười khóc', '🤣': 'cười lăn', '🥰': 'yêu thương', '😍': 'mắt trái tim',
  '😘': 'hôn', '😊': 'cười ngại', '😉': 'nháy mắt', '😎': 'kính râm', '🤔': 'suy nghĩ',
  '😢': 'khóc buồn', '😭': 'khóc nhiều', '😡': 'giận dữ', '😮': 'ngạc nhiên', '🤗': 'ôm',
  '👍': 'thích like', '👎': 'không thích dislike', '🙏': 'cảm ơn cầu nguyện', '👏': 'vỗ tay',
  '❤️': 'trái tim yêu', '💔': 'trái tim vỡ', '🔥': 'lửa tuyệt vời', '🎉': 'chúc mừng', '💯': '100 hoàn hảo'
};

class AppClient {
  constructor() {
    this.currentUser = null;
    this.currentChatPartner = null; // Có thể là User hoặc Group { id, name, avatar, isGroup }
    this.oldestMessageId = 0;
    this.hasMoreMessages = false;
    this.isLoadingOlderMessages = false;
    this.otpTimerInterval = null;
    this.tempRegisterData = null;
    this.currentMedia = { type: 'NONE', url: '' };
    this.shareTargetPostId = null;
    this.heartbeatInterval = null;
    this.searchDebounceTimer = null;
    this.activeModalPostId = null;
    this.currentReactionListData = null;
    this.replyingToCommentId = null;
    this.replyingToAuthorName = null;
    this.currentSuggestionFilter = 'ALL';
    this.currentSuggestionLimit = 20;
    this.fullFriendsCache = [];
    this.pendingRequestsCache = [];
    this.profileFriendsCache = [];
    this.visibleFriendsLimit = 20;
    this.visibleRequestsLimit = 20;
    this.visibleProfileFriendsLimit = 20;
    this.notificationOffset = 0;
    this.notificationHasMore = false;
    this.notificationsLoading = false;
    this.notificationItems = [];
    this.infiniteScrollObservers = new WeakMap();
    this.suggestionsLoading = false;
    this.suggestionsReloadQueued = false;
    this.selectedHobbies = [];
    this.hobbyTargetInputId = '';
    this.currentContactFilter = 'all';
    this.quickReactionEmoji = '👍';
    this.quickReactionValues = [...DEFAULT_CHAT_QUICK_REACTIONS];
    this.quickEmojiPressState = null;
    this.quickEmojiPressTimers = [];
    this.suppressQuickEmojiClickUntil = 0;
    this.allContactsCache = null;
    this.pendingMessageContacts = [];
    this.adminReportsCache = null;
    this.adminPostsCache = null;
    this.typingTimer = null;
    this.isTyping = false;
    this.typingPollInterval = null;
    this.msgReadPollInterval = null;
    this.messageReadObserver = null;
    this.messageReadVisibilityTimers = new Map();
    this.pendingReadMessageIds = new Set();
    this.readMessageIds = new Set();
    this.readReceiptFlushTimer = null;
    this.readReceiptRequestInFlight = false;
    this.messagePollInterval = null;
    this.replyingToMessage = null;
    this.lastRenderedMsgSignature = '';
    this.messageScrollAnchor = null;
    this.isViewingHistory = false;      // Đang xem ngữ cảnh cũ sau khi nhảy tới tin nhắn gốc
    this.msgCache = {};                 // Cache tin nhắn đã render theo ID (dùng cho Reply)
    this.messageTimestampTimers = new Map();
    this.chatJumpLayoutObserver = null;
    this.chatJumpLayoutTimeout = null;
    this.chatSearchResults = [];
    this.chatSearchOffset = 0;
    this.chatSearchHasMore = false;
    this.chatSearchLoading = false;
    this.chatSearchRequestId = 0;
    this.chatSearchScrollBound = false;
    this.REPLY_SNIPPET_MAX = 40;        // Số ký tự tối đa của đoạn trích dẫn Reply
    this._chatScrollBound = false;
    this.presenceSessionId = null;      // ID phiên (mỗi tab một ID) gửi kèm heartbeat
    this.presencePollInterval = null;
    this.onlineIds = new Set();         // Danh sách bạn bè đang online do server trả về
    this._presenceBound = false;
    this._presenceStopped = false;

    // Phân trang bài viết (Feed: 10 ban đầu, load 7; Profile: 5 ban đầu, load 5)
    this.feedLimit = 10;
    this.feedOffset = 0;
    this.feedHasMore = true;
    this.isLoadingFeed = false;
    this.deferredFeedPostIds = [];
    this.profileLimit = 5;
    this.profileOffset = 0;
    this.profileHasMore = true;
    this.isLoadingProfile = false;
    this.profileUserId = null;

    this.init();
  }

  renderFeedFriendSuggestions() {
    return `
      <section class="feed-friend-suggestions" id="feedFriendSuggestions">
        <div class="feed-friend-suggestions-header">
          <h3><span aria-hidden="true">👥</span> Những người bạn có thể biết</h3>
          <button type="button" class="feed-suggestion-scroll-btn" onclick="app.scrollFeedSuggestions(1)" aria-label="Xem thêm gợi ý">›</button>
        </div>
        <div class="feed-friend-suggestions-track" id="feedFriendSuggestionsTrack">
          <div class="feed-suggestions-loading">Đang tải gợi ý bạn bè...</div>
        </div>
        <a class="feed-friend-suggestions-all" href="friends.html?tab=suggestions">Xem tất cả</a>
      </section>
    `;
  }

  async loadFeedFriendSuggestions() {
    const track = document.getElementById('feedFriendSuggestionsTrack');
    if (!track || !this.currentUser) return;
    try {
      const response = await fetch(`/api/friends/suggest?user_id=${encodeURIComponent(this.currentUser.id)}&filter=ALL&limit=12`);
      const data = await response.json();
      if (!response.ok || data.status !== 'ok') {
        throw new Error(data.message || 'Không thể tải gợi ý bạn bè.');
      }
      const suggestions = Array.isArray(data.suggestions) ? data.suggestions : [];
      if (!suggestions.length) {
        track.innerHTML = '<div class="feed-suggestions-loading">Chưa có gợi ý bạn bè phù hợp.</div>';
        return;
      }

      track.innerHTML = suggestions.map(suggestion => {
        const id = this.escapeHtml(suggestion.id || '');
        const name = this.escapeHtml(suggestion.name || 'Người dùng');
        const avatar = this.escapeHtml(suggestion.avatar || '/img/default-avatar.svg');
        const target = this.escapeHtml(suggestion.user_token || suggestion.id || '');
        let buttonHtml = '';
        if (suggestion.request_status === 'PENDING_SENT') {
          buttonHtml = `<div id="feed-sug-btn-box-${id}"><button class="feed-suggestion-action btn-secondary" onclick="app.handleCancelFriendRequest(${app.jsArg(id)})">⏳ Đã gửi lời mời</button></div>`;
        } else if (suggestion.request_status === 'PENDING_RECEIVED') {
          buttonHtml = `<button class="feed-suggestion-action btn-primary" onclick="app.respondFriendRequest(${app.jsArg(id)}, 'ACCEPT')">Chấp nhận</button>`;
        } else if (suggestion.request_status === 'FRIEND') {
          buttonHtml = '<span class="feed-suggestion-friend-status">✓ Bạn bè</span>';
        } else {
          buttonHtml = `<div id="feed-sug-btn-box-${id}"><button class="feed-suggestion-action btn-primary" onclick="app.sendFriendRequest(${app.jsArg(id)})">♟ Thêm bạn bè</button></div>`;
        }
        return `
          <article class="feed-suggestion-card" id="feed-sug-${id}">
            <button type="button" class="feed-suggestion-dismiss" onclick="document.getElementById('feed-sug-${id}')?.remove()" aria-label="Ẩn gợi ý">×</button>
            <img class="feed-suggestion-avatar" src="${avatar}" alt="" onclick="app.openUserProfile(${app.jsArg(target)})">
            <div class="feed-suggestion-card-info">
              <button type="button" class="feed-suggestion-name" onclick="app.openUserProfile(${app.jsArg(target)})" title="${name}">${name}</button>
              <div class="feed-suggestion-mutual">${Number(suggestion.mutual) > 0 ? `${Number(suggestion.mutual)} bạn chung` : '&nbsp;'}</div>
              ${buttonHtml}
            </div>
          </article>`;
      }).join('');
      track.onwheel = event => {
        if (Math.abs(event.deltaY) > Math.abs(event.deltaX)) {
          event.preventDefault();
          track.scrollLeft += event.deltaY;
        }
      };
    } catch (error) {
      console.error('Lỗi tải gợi ý bạn bè trong bảng tin:', error);
      track.innerHTML = '<div class="feed-suggestions-loading">Không thể tải gợi ý bạn bè lúc này.</div>';
    }
  }

  scrollFeedSuggestions(direction) {
    const track = document.getElementById('feedFriendSuggestionsTrack');
    if (track) track.scrollBy({ left: direction * Math.max(track.clientWidth * 0.75, 220), behavior: 'smooth' });
  }

  init() {
    this.initTheme();
    this.initVietnamDataOptions();
    this.checkLoginStatus();
    this.setupEventListeners();
    this.setupInfiniteScroll();
  }

  observeInfiniteScrollList(container, onLoadMore, root = null) {
    if (!container || !('IntersectionObserver' in window)) return;
    let sentinel = container.querySelector(':scope > .infinite-scroll-sentinel');
    if (!sentinel) {
      sentinel = document.createElement('div');
      sentinel.className = 'infinite-scroll-sentinel';
      sentinel.innerHTML = '<span class="infinite-scroll-spinner" aria-hidden="true"></span><span>Đang tải thêm...</span>';
      container.appendChild(sentinel);
    }

    let state = this.infiniteScrollObservers.get(container);
    if (state && state.sentinel !== sentinel) {
      state.observer.disconnect();
      state = null;
    }
    if (!state) {
      const observer = new IntersectionObserver(entries => {
        if (entries.some(entry => entry.isIntersecting)) onLoadMore();
      }, { root, rootMargin: '100px 0px' });
      state = { observer, sentinel };
      this.infiniteScrollObservers.set(container, state);
    }
    sentinel.style.display = 'flex';
    state.observer.observe(sentinel);
  }

  stopObservingInfiniteScrollList(container) {
    if (!container) return;
    const sentinel = container.querySelector(':scope > .infinite-scroll-sentinel');
    const state = this.infiniteScrollObservers.get(container);
    if (state) state.observer.disconnect();
    this.infiniteScrollObservers.delete(container);
    if (sentinel) sentinel.style.display = 'none';
  }

  // =========================================================================
  // 1. THEME SÁNG / TỐI & DỮ LIỆU VIỆT NAM (TỈNH THÀNH, TRƯỜNG HỌC, SỞ THÍCH)
  // =========================================================================
  initTheme() {
    const savedTheme = localStorage.getItem('cs_theme') || 'light';
    this.setTheme(savedTheme);

    const toggleBtn = document.getElementById('themeToggleBtn');
    if (toggleBtn) {
      toggleBtn.addEventListener('click', () => {
        const currentTheme = document.documentElement.getAttribute('data-theme') || 'light';
        const newTheme = (currentTheme === 'light') ? 'dark' : 'light';
        this.setTheme(newTheme);
      });
    }
  }

  captureMessageScrollAnchor(msgId) {
    const chatBody = document.getElementById('chatBody');
    const message = document.getElementById(`msg-${msgId}`);
    if (!chatBody || !message) return null;
    return { msgId, top: message.getBoundingClientRect().top };
  }

  restoreMessageScrollAnchor(anchor) {
    if (!anchor) return;
    const chatBody = document.getElementById('chatBody');
    const message = document.getElementById(`msg-${anchor.msgId}`);
    if (chatBody && message) {
      chatBody.scrollTop += message.getBoundingClientRect().top - anchor.top;
    }
  }

  setTheme(theme) {
    document.documentElement.setAttribute('data-theme', theme);
    localStorage.setItem('cs_theme', theme);
    const toggleBtn = document.getElementById('themeToggleBtn');
    if (toggleBtn) {
      toggleBtn.textContent = (theme === 'dark') ? '☀️' : '🌙';
    }
  }

  initSearchableSelect(inputId, dataList) {
    const input = document.getElementById(inputId);
    const dropdown = document.getElementById(`${inputId}-dropdown`);
    if (!input || !dropdown) return;
    if (!Array.isArray(dataList)) return;

    if (input._searchableInitialized) return;
    input._searchableInitialized = true;

    let highlightIndex = -1;

    const renderItems = (query) => {
      const q = (query || '').trim().toLowerCase();
      const matches = q
        ? dataList.filter(v => v.toLowerCase().includes(q)).slice(0, 50)
        : dataList.slice(0, 50);

      if (matches.length === 0) {
        dropdown.innerHTML = '<div class="searchable-dropdown-empty">Không tìm thấy kết quả phù hợp</div>';
      } else {
        dropdown.innerHTML = matches.map((v, i) =>
          `<div class="searchable-dropdown-item" data-value="${this.escapeHtml(v)}" data-index="${i}">${this.escapeHtml(v)}</div>`
        ).join('');

        dropdown.querySelectorAll('.searchable-dropdown-item').forEach(item => {
          item.addEventListener('mousedown', (e) => {
            e.preventDefault();
            input.value = item.dataset.value;
            dropdown.classList.remove('open');
            input.dispatchEvent(new Event('change', { bubbles: true }));
          });
        });
      }
      highlightIndex = -1;
      dropdown.classList.add('open');
    };

    input.addEventListener('focus', () => {
      renderItems(input.value);
    });

    input.addEventListener('input', () => {
      renderItems(input.value);
    });

    input.addEventListener('keydown', (e) => {
      const items = dropdown.querySelectorAll('.searchable-dropdown-item');
      if (!items.length) return;
      if (e.key === 'ArrowDown') {
        e.preventDefault();
        highlightIndex = Math.min(highlightIndex + 1, items.length - 1);
        items.forEach((el, i) => el.classList.toggle('highlighted', i === highlightIndex));
        items[highlightIndex]?.scrollIntoView({ block: 'nearest' });
      } else if (e.key === 'ArrowUp') {
        e.preventDefault();
        highlightIndex = Math.max(highlightIndex - 1, 0);
        items.forEach((el, i) => el.classList.toggle('highlighted', i === highlightIndex));
        items[highlightIndex]?.scrollIntoView({ block: 'nearest' });
      } else if (e.key === 'Enter' && highlightIndex >= 0) {
        e.preventDefault();
        input.value = items[highlightIndex].dataset.value;
        dropdown.classList.remove('open');
        input.dispatchEvent(new Event('change', { bubbles: true }));
      } else if (e.key === 'Escape') {
        dropdown.classList.remove('open');
      }
    });

    document.addEventListener('mousedown', (e) => {
      if (!input.contains(e.target) && !dropdown.contains(e.target)) {
        dropdown.classList.remove('open');
      }
    });
  }

  initVietnamDataOptions() {
    if (typeof VIETNAM_DATA === 'undefined') return;

    // Quê quán & Nơi ở (Datalist hoặc Select hoặc Searchable Input)
    const provDatalists = ['provincesList', 'provincesDatalist'];
    provDatalists.forEach(id => {
      const dl = document.getElementById(id);
      if (dl && dl.children.length === 0) {
        VIETNAM_DATA.provinces.forEach(p => {
          const opt = document.createElement('option');
          opt.value = p;
          dl.appendChild(opt);
        });
      }
    });

    ['profHometownSelect', 'profResidenceSelect', 'editResidence', 'editHometown'].forEach(id => {
      const sel = document.getElementById(id);
      if (!sel) return;
      if (sel.tagName === 'SELECT' && sel.children.length <= 1) {
        VIETNAM_DATA.provinces.forEach(p => {
          const opt = document.createElement('option');
          opt.value = p;
          opt.textContent = p;
          sel.appendChild(opt);
        });
      }
      // Nếu là input (searchable), khởi tạo searchable dropdown
      if (sel.tagName === 'INPUT') {
        this.initSearchableSelect(id, VIETNAM_DATA.provinces);
      }
    });

    // Trường học (Datalist 238 trường)
    const schoolDatalists = ['schoolsList', 'schoolOptionsList'];
    schoolDatalists.forEach(id => {
      const dl = document.getElementById(id);
      if (dl && dl.children.length === 0) {
        VIETNAM_DATA.schools.forEach(s => {
          const opt = document.createElement('option');
          opt.value = s;
          dl.appendChild(opt);
        });
      }
    });

    // Trường học dạng searchable input hoặc select
    const schoolFields = ['profSchoolInput', 'editSchool'];
    schoolFields.forEach(id => {
      const el = document.getElementById(id);
      if (!el) return;
      if (el.tagName === 'SELECT' && el.children.length <= 1) {
        VIETNAM_DATA.schools.forEach(s => {
          const opt = document.createElement('option');
          opt.value = s;
          opt.textContent = s;
          el.appendChild(opt);
        });
      } else if (el.tagName === 'INPUT') {
        this.initSearchableSelect(id, VIETNAM_DATA.schools);
      }
    });

    // Sở thích (Datalist 283 sở thích)
    const hobbyDatalists = ['hobbiesList', 'hobbiesOptionsList'];
    hobbyDatalists.forEach(id => {
      const dl = document.getElementById(id);
      if (dl && dl.children.length === 0) {
        VIETNAM_DATA.hobbies.forEach(h => {
          const opt = document.createElement('option');
          opt.value = h;
          dl.appendChild(opt);
        });
      }
    });

    const hobbySelect = document.getElementById('profHobbyQuickSelect');
    if (hobbySelect && hobbySelect.children.length <= 1) {
      VIETNAM_DATA.hobbies.forEach(h => {
        const opt = document.createElement('option');
        opt.value = h;
        opt.textContent = h;
        hobbySelect.appendChild(opt);
      });
    }
  }

  addHobbyToInput(hobby) {
    if (!hobby) return;
    const input = document.getElementById('profInterestsInput') || document.getElementById('editInterests');
    if (!input) return;
    let curr = input.value.trim();
    if (curr.length > 0 && !curr.endsWith(';')) {
      curr += ';';
    }
    if (!curr.includes(hobby)) {
      input.value = curr ? (curr + hobby) : hobby;
    }
  }

  // =========================================================================
  // 2. AUTHENTICATION & TRẠNG THÁI ONLINE
  // =========================================================================
  checkLoginStatus() {
    let stored = localStorage.getItem('cs_user');
    // BẢO MẬT: không tự động đăng nhập bất kỳ tài khoản nào. Chưa có phiên thì hiện màn hình đăng nhập.

    if (stored) {
      try {
        this.currentUser = JSON.parse(stored);
        if (this.currentUser.role === 'ADMIN') {
          const pth = (window.location.pathname || '').toLowerCase();
          if (!pth.includes('admin.html')) { window.location.replace('admin.html'); return; }
        }
        this.renderUserUI();
        this.startHeartbeat();

        const authModal = document.getElementById('authModal');
        if (authModal) authModal.style.display = 'none';

        // Tải các widget nếu đang ở trang chủ index.html
        if (document.getElementById('postsList') && !window.location.pathname.includes('profile.html')) {
          const postIdFromHash = window.location.hash.match(/^#post-(.+)$/)?.[1];
          this.loadFeed().then(() => {
            if (postIdFromHash) this.showPostInFeed(decodeURIComponent(postIdFromHash));
          });
          this.loadSuggestions();
          this.loadFriendsAndContacts();
          this.loadNotifications();
        } else if (window.location.pathname.includes('profile.html')) {
          this.loadNotifications();
        }
        if (document.getElementById('contactsContainer') && !document.getElementById('postsList')) {
          this.loadFriendsAndContacts();
        }

        // Hiện nút Admin nếu có quyền ADMIN — và tách giao diện
        if (this.currentUser.role === 'ADMIN') {
          const admBtn = document.getElementById('adminPanelBtn');
          if (admBtn) admBtn.style.display = 'flex';
          const sideAdm = document.getElementById('sideAdminItem');
          if (sideAdm) sideAdm.style.display = 'flex';
          // Admin chỉ dùng dashboard — không dùng feed/bạn bè/nhóm
          const path = (window.location.pathname || '').toLowerCase();
          const onAdminPage = path.includes('admin.html');
          if (!onAdminPage) {
            window.location.href = 'admin.html';
            return;
          }
        } else {
          // User thường không vào trang admin
          const path = (window.location.pathname || '').toLowerCase();
          if (path.includes('admin.html')) {
            this.showToast('Bạn không có quyền truy cập trang quản trị!', 'error');
            window.location.href = 'index.html';
            return;
          }
        }
        this.refreshSidebarFriendBadge();
      } catch (e) {
        this.currentUser = null;
        this.showAuthModal();
      }
    } else {
      this.showAuthModal();
    }
  }

  showAuthModal() {
    const modal = document.getElementById('authModal');
    if (modal) modal.style.display = 'flex';
  }

  renderUserUI() {
    if (!this.currentUser) return;
    const u = this.currentUser;
    this.quickReactionValues = this.getQuickReactionValues();
    this.quickReactionEmoji = this.getSavedQuickReactionEmoji();
    this.updateQuickReactionButton();

    const navAvatar = document.getElementById('navAvatar');
    if (navAvatar) navAvatar.src = u.avatar || '/img/default-avatar.svg';

    const navFullname = document.getElementById('navFullname');
    if (navFullname) navFullname.textContent = u.fullname;

    const sideAvatar = document.getElementById('sideAvatar');
    if (sideAvatar) sideAvatar.src = u.avatar || '/img/default-avatar.svg';

    const sideFullname = document.getElementById('sideFullname');
    if (sideFullname) sideFullname.textContent = u.fullname;

    const sideBio = document.getElementById('sideBio');
    if (sideBio) sideBio.textContent = u.bio || 'Chưa cập nhật tiểu sử';

    const sideHometown = document.getElementById('sideHometown');
    if (sideHometown) sideHometown.textContent = u.hometown || '--';

    const sideResidence = document.getElementById('sideResidence');
    if (sideResidence) sideResidence.textContent = u.residence || '--';

    const sideSchool = document.getElementById('sideSchool');
    if (sideSchool) sideSchool.textContent = u.school || '--';

    const sideWorkplace = document.getElementById('sideWorkplace');
    if (sideWorkplace) sideWorkplace.textContent = u.workplace || '--';

    const sideInterests = document.getElementById('sideInterests');
    if (sideInterests) sideInterests.textContent = this.formatInterests(u.interests);

    const sideGender = document.getElementById('sideGender');
    if (sideGender) sideGender.textContent = u.gender || '--';

    const postCreateAvatar = document.getElementById('postCreateAvatar');
    if (postCreateAvatar) postCreateAvatar.src = u.avatar || '/img/default-avatar.svg';
  }

  formatInterests(str) {
    if (!str) return '--';
    return str.split(/[;,]/).map(s => s.trim()).filter(Boolean).join(', ');
  }

  // --- TRẠNG THÁI ONLINE THẬT (đồng bộ với backend /api/users/heartbeat, /offline, /api/presence/friends) ---
  getPresenceSessionId() {
    let sid = null;
    try { sid = sessionStorage.getItem('cs_presence_sid'); } catch (_) {}
    if (!sid) {
      sid = 's' + Math.random().toString(36).slice(2, 10) + Date.now().toString(36);
      try { sessionStorage.setItem('cs_presence_sid', sid); } catch (_) {}
    }
    return sid;
  }

  startHeartbeat() {
    this.stopHeartbeat();
    this._presenceStopped = false;
    this.presenceSessionId = this.getPresenceSessionId();
    this.heartbeatTick = 0;
    this._sendPulse = () => {
      if (!this.currentUser || this._presenceStopped) return;
      this.heartbeatTick++;
      fetch('/api/users/heartbeat', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ user_id: this.currentUser.id, session_id: this.presenceSessionId }),
        keepalive: true
      }).then(() => {
        // Làm mới danh bạ (tin nhắn mới nhất...) khoảng 30 giây/lần như trước
        if (this.heartbeatTick % 3 === 1 && document.getElementById('contactsContainer')) {
          this.loadFriendsAndContacts();
        }
      }).catch(() => {});
    };
    this._sendPulse();
    this.heartbeatInterval = setInterval(this._sendPulse, 10000); // 10 giây/lần; server coi là offline nếu quá 45 giây không có tín hiệu
    this.startPresencePolling();
    this.bindPresenceLifecycle();
  }

  stopHeartbeat() {
    this._presenceStopped = true;
    if (this.heartbeatInterval) { clearInterval(this.heartbeatInterval); this.heartbeatInterval = null; }
    if (this.presencePollInterval) { clearInterval(this.presencePollInterval); this.presencePollInterval = null; }
  }

  startPresencePolling() {
    if (this.presencePollInterval) clearInterval(this.presencePollInterval);
    this.pollPresence();
    this.presencePollInterval = setInterval(() => this.pollPresence(), 3000);
  }

  async pollPresence() {
    if (!this.currentUser || this.currentUser.role === 'ADMIN' || this._presenceStopped || document.hidden) return;
    try {
      const res = await fetch(`/api/presence/friends?user_id=${encodeURIComponent(this.currentUser.id)}&_=${Date.now()}`, { cache: 'no-store' });
      const data = await res.json();
      if (data.status === 'ok' && Array.isArray(data.online_ids)) {
        this.applyPresence(new Set(data.online_ids));
      }
    } catch (_) {}
  }

  // Cập nhật chấm xanh tại chỗ (không vẽ lại cả danh sách)
  applyPresence(onlineSet) {
    this.onlineIds = onlineSet;
    if (this.allContactsCache && Array.isArray(this.allContactsCache.friends)) {
      this.allContactsCache.friends.forEach(f => { f.is_online = onlineSet.has(f.id); });
    }
    document.querySelectorAll('.contact-item[data-uid]').forEach(el => {
      const dot = el.querySelector('.online-indicator');
      if (!dot) return;
      const on = onlineSet.has(el.dataset.uid);
      dot.classList.toggle('online', on);
      dot.classList.toggle('offline', !on);
    });
    // Đổi trạng thái của người đang chat -> cập nhật dòng "Đang hoạt động" trên header
    if (this.currentChatPartner && !this.currentChatPartner.isGroup) {
      const key = this.currentChatPartner.id + ':' + onlineSet.has(this.currentChatPartner.id);
      if (this._lastPartnerPresenceKey !== key) {
        this._lastPartnerPresenceKey = key;
        this.updateChatHeaderStatus();
      }
    }
  }

  // Báo server chuyển sang offline. all=true: đăng xuất khỏi mọi tab.
  sendOffline(all = false) {
    if (!this.currentUser) return Promise.resolve();
    const payload = JSON.stringify({
      user_id: this.currentUser.id,
      session_id: this.presenceSessionId || this.getPresenceSessionId(),
      all: !!all
    });
    try {
      if (!all && navigator.sendBeacon) {
        navigator.sendBeacon('/api/users/offline', new Blob([payload], { type: 'application/json' }));
        return Promise.resolve();
      }
    } catch (_) {}
    return fetch('/api/users/offline', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: payload,
      keepalive: true
    }).catch(() => {});
  }

  bindPresenceLifecycle() {
    if (this._presenceBound) return;
    this._presenceBound = true;
    const resume = () => {
      if (!this.currentUser || this._presenceStopped) return;
      if (this._sendPulse) this._sendPulse();
      this.pollPresence();
    };
    // Đóng tab / chuyển trang: báo offline ngay (sendBeacon)
    window.addEventListener('pagehide', () => { if (!this._presenceStopped) this.sendOffline(false); });
    window.addEventListener('pageshow', (e) => { if (e.persisted) resume(); });
    document.addEventListener('visibilitychange', () => { if (!document.hidden) resume(); });
    window.addEventListener('online', resume);
    // Đăng xuất ở tab khác -> tab này ngừng gửi heartbeat và quay về màn hình đăng nhập
    window.addEventListener('storage', (e) => {
      if (e.key === 'cs_user' && !e.newValue && this.currentUser) {
        this.stopHeartbeat();
        sessionStorage.setItem('cs_logged_out', '1');
        window.location.reload();
      }
    });
  }

  setupEventListeners() {
    this.quickReactionValues = this.getQuickReactionValues();
    this.quickReactionEmoji = this.getSavedQuickReactionEmoji();
    this.updateQuickReactionButton();
    document.addEventListener('visibilitychange', () => {
      if (document.hidden) this.stopObservingVisibleMessages();
      else this.updateReadReceipt();
    });
    const logoutBtn = document.getElementById('logoutBtn');
    if (logoutBtn) {
      logoutBtn.addEventListener('click', () => this.handleLogout());
    }

    // Biểu tượng tin nhắn trên thanh điều hướng mở trang nhắn tin riêng.
    const quickChatBtn = document.getElementById('quickChatBtn');
    if (quickChatBtn) {
      quickChatBtn.addEventListener('click', (e) => {
        e.stopPropagation();
        window.location.href = 'messenger.html';
      });
    }
    // Khu vực chat đang được mở tạm (màn hình hẹp): bấm ra ngoài thì đóng lại
    document.addEventListener('click', (e) => {
      const sb = document.querySelector('.right-sidebar.force-open');
      if (sb && !sb.contains(e.target) && !e.target.closest('#quickChatBtn') && !e.target.closest('#chatWindow')) {
        sb.classList.remove('force-open');
      }
    });
    if (window.location.hash === '#chat-list') {
      setTimeout(() => this.scrollToChatList(), 700);
    }

    const searchInput = document.getElementById('searchInput');
    if (searchInput) {
      searchInput.addEventListener('focus', () => {
        if (!searchInput.value.trim()) {
          this.showSearchHistory();
        }
      });
      searchInput.addEventListener('click', () => {
        if (!searchInput.value.trim()) {
          this.showSearchHistory();
        }
      });
    }

    // Đóng dropdown khi click ra ngoài
    document.addEventListener('click', (e) => {
      const notifWrapper = document.querySelector('.nav-dropdown-wrapper');
      const notifDropdown = document.getElementById('notificationDropdown');
      if (notifDropdown && notifWrapper && !notifWrapper.contains(e.target)) {
        notifDropdown.style.display = 'none';
      }

      const searchWrapper = document.querySelector('.search-box-wrapper');
      const searchDropdown = document.getElementById('searchSuggestionsDropdown');
      if (searchDropdown && searchWrapper && !searchWrapper.contains(e.target)) {
        searchDropdown.style.display = 'none';
      }

      const chatSearchBar = document.getElementById('chatSearchBar');
      const chatSearchDropdown = document.getElementById('chatSearchResultsDropdown');
      if (chatSearchDropdown && chatSearchBar && !chatSearchBar.contains(e.target)) {
        chatSearchDropdown.style.display = 'none';
      }

      const chatMoreMenu = document.querySelector('.chat-more-menu-container');
      const chatMoreDropdown = document.getElementById('chatMoreDropdown');
      if (chatMoreDropdown && chatMoreMenu && !chatMoreMenu.contains(e.target)) {
        chatMoreDropdown.style.display = 'none';
      }

      const messageSearchBox = document.querySelector('.all-message-search');
      const messageSearchResults = document.getElementById('allMessagesSearchResults');
      if (messageSearchResults && messageSearchBox && !messageSearchBox.contains(e.target)) {
        messageSearchResults.style.display = 'none';
      }

      const mediaTrigger = e.target.closest('[data-media-viewer]');
      if (mediaTrigger) {
        this.openMediaViewer(mediaTrigger.dataset.mediaViewer, mediaTrigger.dataset.mediaUrl, e);
      }
      const globalMessageResult = e.target.closest('.all-message-search-result');
      if (globalMessageResult) {
        this.openGlobalMessageResult(globalMessageResult);
      }
      if (!e.target.closest('.contact-options-wrap') && !e.target.closest('.contact-options-menu')) {
        this.closeContactOptionsMenus();
      }
      if (!e.target.closest('.chat-list-menu-wrap') && !e.target.closest('.chat-list-menu')) {
        this.closeChatListMenu();
      }

      // Đóng menu bài viết và menu comment khi bấm ra ngoài
      if (!e.target.closest('.post-more-wrapper')) {
        document.querySelectorAll('.post-more-dropdown.show').forEach(el => el.classList.remove('show'));
      }
      if (!e.target.closest('.comment-more-wrapper')) {
        document.querySelectorAll('.comment-more-dropdown.show').forEach(el => el.classList.remove('show'));
      }
      if (!e.target.closest('.msg-circle-btn') && !e.target.closest('.msg-more-dropdown') && !e.target.closest('.msg-reaction-picker') && !e.target.closest('.chat-composer-emoji-wrap') && !e.target.closest('.chat-emoji-modal-overlay')) {
        this.closeAllMessageDropdowns();
      }
    });

    document.addEventListener('keydown', (e) => {
      if (e.key === 'Escape') {
        this.closeMediaViewer();
        this.closeImageLightbox();
        this.closeChatEmojiModal();
      }
    });

    document.addEventListener('pointerdown', (event) => {
      const button = event.target.closest('.chat-composer-like');
      if (!button || event.button !== 0) return;
      event.preventDefault();
      this.clearQuickEmojiPressTimers();
      this.quickEmojiPressState = { button, startedAt: Date.now(), size: 1 };
      this.quickEmojiPressTimers = [
        window.setTimeout(() => this.setQuickEmojiPressSize(2), 400),
        window.setTimeout(() => this.setQuickEmojiPressSize(3), 900)
      ];
    });
    document.addEventListener('pointerup', (event) => {
      if (!this.quickEmojiPressState) return;
      const { button, size } = this.quickEmojiPressState;
      this.quickEmojiPressState = null;
      this.clearQuickEmojiPressTimers();
      button.classList.remove('is-pressed');
      button.style.removeProperty('transform');
      this.suppressQuickEmojiClickUntil = Date.now() + 500;
      if (event.button === 0) this.sendChatLike(size);
    });
    document.addEventListener('pointercancel', () => this.cancelQuickEmojiPress());
    document.addEventListener('click', (event) => {
      const button = event.target.closest('.chat-composer-like');
      if (!button) return;
      event.preventDefault();
      if (Date.now() >= this.suppressQuickEmojiClickUntil) this.sendChatLike(1);
    });
  }

  clearQuickEmojiPressTimers() {
    this.quickEmojiPressTimers.forEach(timer => clearTimeout(timer));
    this.quickEmojiPressTimers = [];
  }

  setQuickEmojiPressSize(size) {
    if (!this.quickEmojiPressState) return;
    this.quickEmojiPressState.size = size;
    const scale = size === 2 ? 1.35 : 1.7;
    this.quickEmojiPressState.button.classList.add('is-pressed');
    this.quickEmojiPressState.button.style.transform = `scale(${scale})`;
  }

  cancelQuickEmojiPress() {
    if (this.quickEmojiPressState?.button) {
      this.quickEmojiPressState.button.classList.remove('is-pressed');
      this.quickEmojiPressState.button.style.removeProperty('transform');
    }
    this.quickEmojiPressState = null;
    this.clearQuickEmojiPressTimers();
  }

  quickEmojiStorageKey() {
    return `cs_quick_chat_reactions_${this.currentUser?.id || 'default'}`;
  }

  getQuickReactionValues() {
    try {
      const saved = JSON.parse(localStorage.getItem(this.quickEmojiStorageKey()) || 'null');
      if (Array.isArray(saved)) {
        const values = [...new Set(saved.filter(value => typeof value === 'string' && value.trim()))].slice(0, 6);
        if (values.length) return values;
      }
    } catch (error) {
      console.error('Không thể tải biểu tượng cảm xúc nhanh:', error);
    }
    return [...DEFAULT_CHAT_QUICK_REACTIONS];
  }

  getSavedQuickReactionEmoji() {
    try {
      const saved = localStorage.getItem(`${this.quickEmojiStorageKey()}_selected`);
      return saved || DEFAULT_CHAT_QUICK_REACTIONS[0];
    } catch (error) {
      console.error('Không thể tải biểu tượng gửi nhanh:', error);
      return DEFAULT_CHAT_QUICK_REACTIONS[0];
    }
  }

  updateQuickReactionButton() {
    document.querySelectorAll('.chat-composer-like').forEach(button => {
      button.textContent = this.quickReactionEmoji || '👍';
      button.title = `Gửi nhanh ${this.quickReactionEmoji || '👍'} (nhấn giữ để phóng to)`;
      button.setAttribute('aria-label', button.title);
    });
  }

  async changeQuickReaction(emoji, targetId) {
    const previousEmoji = this.quickReactionEmoji;
    this.quickReactionValues = [emoji, ...this.getQuickReactionValues().filter(value => value !== emoji)].slice(0, 6);
    this.quickReactionEmoji = emoji;
    try {
      localStorage.setItem(this.quickEmojiStorageKey(), JSON.stringify(this.quickReactionValues));
      localStorage.setItem(`${this.quickEmojiStorageKey()}_selected`, emoji);
    } catch (error) {
      console.error('Không thể lưu biểu tượng cảm xúc nhanh:', error);
      this.showToast('Không thể lưu biểu tượng cảm xúc nhanh trên thiết bị này.', 'error');
    }
    this.closeChatEmojiModal();
    this.updateQuickReactionButton();
    if (previousEmoji !== emoji) await this.sendQuickReactionChangeNotice(emoji);
    if (targetId) this.closeAllMessageDropdowns();
  }

  async sendQuickReactionChangeNotice(emoji) {
    if (!this.currentChatPartner || !this.currentUser) return;
    const body = {
      sender_id: this.currentUser.id,
      content: `__SYS_QUICK_EMOJI__:${emoji}`,
      media_type: 'SYSTEM'
    };
    const isGroup = !!this.currentChatPartner.isGroup;
    if (isGroup) body.group_id = this.currentChatPartner.id;
    else body.recipient_id = this.currentChatPartner.id;
    try {
      const response = await fetch(isGroup ? '/api/groups/messages/send' : '/api/messages/send', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(body)
      });
      const data = await response.json();
      if (!response.ok || data.status !== 'ok') throw new Error(data.message || 'Không thể gửi thông báo đổi biểu tượng.');
      if (isGroup) await this.loadGroupMessages(this.currentChatPartner.id);
      else await this.loadInitialMessages();
      this.loadFriendsAndContacts();
    } catch (error) {
      console.error('Không thể gửi thông báo đổi biểu tượng:', error);
      this.showToast(error.message || 'Không thể thông báo việc đổi biểu tượng.', 'error');
    }
  }

  // Cuộn mượt tới khu vực Đoạn Chat; tự mở ra nếu đang bị thu gọn/ẩn
  scrollToChatList() {
    const widget = document.getElementById('chatListWidget');
    if (!widget) {
      window.location.href = 'index.html#chat-list';
      return;
    }
    if (widget.classList.contains('collapsed')) widget.classList.remove('collapsed');
    const list = document.getElementById('contactsContainer');
    if (list && list.style.display === 'none') list.style.display = '';
    const sidebar = widget.closest('.right-sidebar');
    if (sidebar && window.getComputedStyle(sidebar).display === 'none') {
      sidebar.classList.add('force-open');
    }
    requestAnimationFrame(() => {
      widget.scrollIntoView({ behavior: 'smooth', block: 'start' });
      widget.classList.remove('chat-list-flash');
      void widget.offsetWidth;
      widget.classList.add('chat-list-flash');
      setTimeout(() => widget.classList.remove('chat-list-flash'), 1800);
    });
  }

  async handleLogout() {
    const confirmed = await this.showConfirm('Bạn có chắc chắn muốn đăng xuất?');
    if (!confirmed) return;
    // Ngừng heartbeat và báo server chuyển sang OFFLINE trước khi rời trang
    this.stopHeartbeat();
    await this.sendOffline(true);
    try { await fetch('/api/auth/logout', { method: 'POST', headers: { 'X-Admin-Token': localStorage.getItem('cs_admin_token') || '' } }); } catch (_) {}
    localStorage.removeItem('cs_user');
    localStorage.removeItem('cs_admin_token');
    sessionStorage.setItem('cs_logged_out', '1');
    window.location.href = 'index.html';
  }

  // =========================================================================
  // 3. THỜI GIAN ĐĂNG BÀI CHUẨN CONSOLE (timeAgo)
  // =========================================================================
  formatTimeAgo(timestamp) {
    if (!timestamp) return 'Vừa xong';
    // timestamp: "YYYY-MM-DD HH:MM:SS"
    const parts = timestamp.split(/[- :]/);
    if (parts.length < 6) return timestamp;

    const postDate = new Date(parts[0], parts[1] - 1, parts[2], parts[3], parts[4], parts[5]);
    const now = new Date();
    const diffSec = Math.floor((now.getTime() - postDate.getTime()) / 1000);

    if (diffSec < 60) {
      return "Vừa xong";
    } else if (diffSec < 3600) {
      const mins = Math.max(1, Math.floor(diffSec / 60));
      return `${mins} phút trước`;
    } else if (diffSec < 86400) {
      const hours = Math.floor(diffSec / 3600);
      return `${hours} giờ trước`;
    } else if (diffSec < 30 * 86400) {
      const days = Math.floor(diffSec / 86400);
      return `${days} ngày trước`;
    } else if (diffSec < 365 * 86400) {
      const months = Math.floor(diffSec / (30 * 86400));
      return `${months} tháng trước`;
    } else {
      return `${parts[2]}/${parts[1]}/${parts[0]}`;
    }
  }

  // =========================================================================
  // 4. BÀI ĐĂNG (NEWSFEED) & CỤM ICON CẢM XÚC (HÌNH 1: TỐI ĐA 3 ICON)
  // =========================================================================
  setupInfiniteScroll() {
    window.addEventListener('scroll', () => {
      const scrollPosition = window.innerHeight + window.scrollY;
      const threshold = document.documentElement.scrollHeight - 280;
      if (scrollPosition >= threshold) {
        if (window.location.pathname.includes('profile.html')) {
          const postsTab = document.getElementById('profPostsContainer');
          if (postsTab && postsTab.style.display !== 'none' && this.profileUserId) {
            this.loadUserPosts(this.profileUserId, true);
          }
        } else if (document.getElementById('postsList') && !window.location.pathname.includes('admin.html') && !window.location.pathname.includes('search.html')) {
          this.loadFeed(true);
        }
      }
    });
  }

  async refreshSinglePostCard(postId) {
    if (!postId) return;
    try {
      const res = await fetch(`/api/posts/detail?id=${encodeURIComponent(postId)}&current_user_id=${this.currentUser ? this.currentUser.id : ''}`);
      const data = await res.json();
      if (data.status === 'ok' && data.post) {
        if (!this.postsMap) this.postsMap = {};
        this.postsMap[data.post.id] = data.post;
        const oldCard = document.getElementById(`post-${postId}`);
        if (oldCard) {
          const tempDiv = document.createElement('div');
          tempDiv.innerHTML = this.renderPostCard(data.post);
          const newCard = tempDiv.firstElementChild;
          if (newCard) oldCard.replaceWith(newCard);
        }
      }
    } catch (e) {
      console.error('Lỗi cập nhật bài viết tại chỗ:', e);
    }
  }

  async loadFeed(isLoadMore = false) {
    const container = document.getElementById('postsList');
    if (!container || !this.currentUser) return;

    if (!isLoadMore) {
      this.feedOffset = 0;
      this.feedHasMore = true;
      this.isLoadingFeed = true;
      try {
        const res = await fetch(`/api/posts?user_id=${this.currentUser.id}&limit=10&offset=0`);
        const data = await res.json();

        if (data.status === 'ok') {
          if (!data.posts || data.posts.length === 0) {
            container.innerHTML = '<div style="text-align: center; padding: 2rem; color: var(--text-muted);">Chưa có bài viết nào. Hãy chia sẻ khoảnh khắc đầu tiên!</div>';
            this.feedHasMore = false;
            return;
          }
          this.feedOffset = data.posts.length;
          if (data.posts.length < 10) this.feedHasMore = false;
          const renderedPosts = data.posts.map(post => this.renderPostCard(post));
          if (renderedPosts.length >= 2) {
            renderedPosts.splice(2, 0, this.renderFeedFriendSuggestions());
          }
          container.innerHTML = renderedPosts.join('') +
            '<div id="feedScrollLoader" class="feed-scroll-loader" style="display:none;"><div class="spinner"></div> Đang tải thêm bài viết...</div>';
          if (renderedPosts.length >= 3) this.loadFeedFriendSuggestions();
        }
      } catch (err) {
        container.innerHTML = '<div style="text-align: center; padding: 2rem; color: var(--accent-red);">Không thể tải bảng tin!</div>';
      } finally {
        this.isLoadingFeed = false;
      }
    } else {
      if (this.isLoadingFeed) return;
      if (!this.feedHasMore) {
        await this.appendDeferredFeedPosts(container);
        return;
      }
      this.isLoadingFeed = true;
      const loader = document.getElementById('feedScrollLoader');
      if (loader) loader.style.display = 'flex';

      try {
        const res = await fetch(`/api/posts?user_id=${this.currentUser.id}&limit=7&offset=${this.feedOffset}`);
        const data = await res.json();
        if (data.status === 'ok' && Array.isArray(data.posts)) {
          if (data.posts.length === 0) {
            this.feedHasMore = false;
          } else {
            this.feedOffset += data.posts.length;
            if (data.posts.length < 7) this.feedHasMore = false;
            const newCardsHtml = data.posts.map(post => this.renderPostCard(post)).join('');
            if (loader) {
              loader.insertAdjacentHTML('beforebegin', newCardsHtml);
            } else {
              container.insertAdjacentHTML('beforeend', newCardsHtml);
            }
          }
        }
        await this.appendDeferredFeedPosts(container);
      } catch (err) {
        console.error('Lỗi tải thêm bài viết feed:', err);
      } finally {
        if (loader) loader.style.display = 'none';
        this.isLoadingFeed = false;
      }
    }
  }

  async appendDeferredFeedPosts(container) {
    if (!container || !this.deferredFeedPostIds.length) return;

    for (const postId of [...this.deferredFeedPostIds]) {
      if (document.getElementById(`post-${postId}`)) {
        this.deferredFeedPostIds = this.deferredFeedPostIds.filter(id => id !== postId);
        continue;
      }

      try {
        const res = await fetch(`/api/posts/detail?id=${encodeURIComponent(postId)}&current_user_id=${this.currentUser ? this.currentUser.id : ''}`);
        const data = await res.json();
        if (data.status !== 'ok' || !data.post) {
          throw new Error(data.message || `Không thể tải bài viết ${postId}`);
        }

        const markup = this.renderPostCard(data.post);
        if (!container.querySelector('.post-card')) container.innerHTML = '';
        const loader = document.getElementById('feedScrollLoader');
        if (loader) loader.insertAdjacentHTML('beforebegin', markup);
        else container.insertAdjacentHTML('beforeend', markup);
        this.deferredFeedPostIds = this.deferredFeedPostIds.filter(id => id !== postId);
      } catch (err) {
        console.error('Lỗi tải bài vừa đăng vào bảng tin:', err);
      }
    }
  }

  async showPostInFeed(postId) {
    const container = document.getElementById('postsList');
    if (!container || !postId) return;

    try {
      const res = await fetch(`/api/posts/detail?id=${encodeURIComponent(postId)}&current_user_id=${this.currentUser ? this.currentUser.id : ''}`);
      const data = await res.json();
      if (data.status !== 'ok' || !data.post) {
        this.showToast(data.message || 'Không thể tìm thấy bài viết này!', 'error');
        return;
      }

      let postCard = document.getElementById(`post-${postId}`);
      if (!postCard) {
        if (!container.querySelector('.post-card')) container.innerHTML = '';
        const loader = document.getElementById('feedScrollLoader');
        const markup = this.renderPostCard(data.post);
        if (loader) loader.insertAdjacentHTML('beforebegin', markup);
        else container.insertAdjacentHTML('afterbegin', markup);
        postCard = document.getElementById(`post-${postId}`);
      }

      if (postCard) {
        postCard.scrollIntoView({ behavior: 'smooth', block: 'center' });
        postCard.classList.add('post-notification-target');
        window.setTimeout(() => postCard.classList.remove('post-notification-target'), 2200);
      }
    } catch (err) {
      console.error('Lỗi mở bài viết từ thông báo:', err);
      this.showToast('Không thể tải bài viết từ thông báo!', 'error');
    }
  }

  renderPostCard(p) {
    if (!this.postsMap) this.postsMap = {};
    this.postsMap[p.id] = p;

    // Xử lý icon nút bày tỏ cảm xúc của tôi
    let myReactionIcon = '🤍';
    let myReactionClass = '';
    let myReactionLabel = 'Thích';

    if (p.my_reaction === 'LOVE') { myReactionIcon = '💗'; myReactionClass = 'reacted-love'; myReactionLabel = 'Yêu thích'; }
    else if (p.my_reaction === 'LIKE') { myReactionIcon = '👍'; myReactionClass = 'reacted-like'; myReactionLabel = 'Thích'; }
    else if (p.my_reaction === 'HAHA') { myReactionIcon = '😆'; myReactionClass = 'reacted-haha'; myReactionLabel = 'Haha'; }
    else if (p.my_reaction === 'WOW') { myReactionIcon = '😮'; myReactionClass = 'reacted-wow'; myReactionLabel = 'Wow'; }
    else if (p.my_reaction === 'SAD') { myReactionIcon = '😢'; myReactionClass = 'reacted-sad'; myReactionLabel = 'Buồn'; }
    else if (p.my_reaction === 'ANGRY') { myReactionIcon = '😡'; myReactionClass = 'reacted-angry'; myReactionLabel = 'Phẫn nộ'; }

    // Xử lý cụm icon cảm xúc (Hình 1: Tối đa 3 icon được thả nhiều nhất kèm hover tooltip tên người)
    let reactionsClusterHtml = '';
    const totalCount = p.total_reactions || (p.reactions ? p.reactions.total : 0) || 0;
    if (totalCount > 0 && p.top_reactions && p.top_reactions.length > 0) {
      const top3 = p.top_reactions.slice(0, 3);
      const iconsHtml = top3.map(r => {
        let emoji = r.icon || '👍';
        if (!r.icon) {
          if (r.type === 'LOVE') emoji = '💗';
          else if (r.type === 'HAHA') emoji = '😆';
          else if (r.type === 'WOW') emoji = '😮';
          else if (r.type === 'SAD') emoji = '😢';
          else if (r.type === 'ANGRY') emoji = '😡';
        }
        return `<span class="reaction-cluster-icon">${emoji}</span>`;
      }).join('');

      let allNames = [];
      top3.forEach(r => {
        const uList = r.user_names || r.users || [];
        if (Array.isArray(uList)) {
          uList.forEach(u => { if (!allNames.includes(u)) allNames.push(u); });
        }
      });

      let tooltipText = allNames.length > 0 ? (allNames.slice(0, 4).join(', ') + (allNames.length > 4 ? ` và ${allNames.length - 4} người khác` : '')) : `${totalCount} người đã bày tỏ cảm xúc`;

      reactionsClusterHtml = `
        <div class="reaction-cluster-box" onclick="app.openPostReactionsModal(${app.jsArg(p.id)})" title="${this.escapeHtml(tooltipText)}">
          <div class="reaction-cluster-icons">${iconsHtml}</div>
          <span>${totalCount}</span>
          <div class="reaction-names-tooltip">${this.escapeHtml(tooltipText)}</div>
        </div>
      `;
    } else {
      reactionsClusterHtml = `<span style="color: var(--text-muted); font-size: 0.85rem;">Hãy là người đầu tiên bày tỏ cảm xúc</span>`;
    }

    // Media (Ảnh hoặc Video)
    const mediaHtml = this.renderPostMedia(p.media_type, p.media_url);

    // Thời gian đăng bài theo định dạng console timeAgo
    const timeDisplay = this.formatTimeAgo(p.created_at);
    const commentCount = Number.isFinite(p.comments_count)
      ? p.comments_count
      : ((p.comments && Array.isArray(p.comments)) ? p.comments.length : 0);
    const isAuthor = this.currentUser && this.currentUser.id === p.author_id;
    const profileTarget = p.author_token || p.author_id;

    let visIcon = '🌐';
    if (p.visibility === 'FRIENDS') visIcon = '👥';
    else if (p.visibility === 'PRIVATE') visIcon = '🔒';

    const titleHtml = p.title ? `<div class="post-card-title">${this.escapeHtml(p.title)}</div>` : '';

    // Xử lý bài viết đăng lại (Repost)
    const isRepost = !!p.shared_post_id;
    let repostBadgeHtml = '';
    let bodyContentHtml = '';

    if (isRepost) {
      repostBadgeHtml = `<span class="repost-header-badge">🔁 Đã đăng lại</span>`;
      let thoughtsHtml = p.content ? `<div class="post-content repost-thoughts">${this.escapeHtml(p.content)}</div>` : '';
      let origHtml = '';

      if (p.original_post) {
        const op = p.original_post;
        const origMediaHtml = this.renderPostMedia(op.media_type, op.media_url, 'repost-orig-media');

        origHtml = `
          <div class="repost-original-card">
            <div class="repost-orig-header">
              <div class="repost-orig-author-info" onclick="app.openUserProfile(${app.jsArg(op.author_token || op.author_id)})" style="cursor: pointer;">
                <img src="${app.escapeHtml(op.author_avatar || '/img/default-avatar.svg')}" class="repost-orig-avatar" alt="Avatar">
                <div>
                  <div class="repost-orig-name">${this.escapeHtml(op.author_name)}</div>
                  <div class="repost-orig-time">🕒 ${this.formatTimeAgo(op.created_at)}</div>
                </div>
              </div>
              <button class="btn-view-orig-post" onclick="app.openCommentModal(${app.jsArg(op.id)})" title="Xem chi tiết bài viết gốc">
                Xem gốc ↗
              </button>
            </div>
            ${op.title ? `<div class="repost-orig-title">${this.escapeHtml(op.title)}</div>` : ''}
            <div class="repost-orig-content">${this.escapeHtml(op.content || '')}</div>
            ${origMediaHtml}
            <div class="repost-orig-footer">
              <span onclick="app.openCommentModal(${app.jsArg(op.id)})" style="cursor: pointer;">💬 ${op.comments_count || 0} bình luận &bull; ❤️ ${op.total_reactions || 0} cảm xúc</span>
            </div>
          </div>
        `;
      } else {
        origHtml = `
          <div class="repost-original-card deleted">
            <em>⚠️ Bài viết gốc đã bị xóa hoặc không còn khả dụng trên hệ thống.</em>
          </div>
        `;
      }
      bodyContentHtml = `${thoughtsHtml}${origHtml}`;
    } else {
      bodyContentHtml = `
        ${titleHtml}
        <div class="post-content">${this.escapeHtml(p.content)}</div>
        ${mediaHtml}
      `;
    }

    return `
      <article class="post-card" id="post-${p.id}">
        <div class="post-header">
          <div class="post-header-left" onclick="app.openUserProfile(${app.jsArg(profileTarget)})" style="cursor: pointer;">
            <img src="${app.escapeHtml(p.author_avatar || '/img/default-avatar.svg')}" alt="Avatar">
            <div>
              <div class="post-author-name">${this.escapeHtml(p.author_name)} ${repostBadgeHtml}${p.moderation_action === 'FLAG' ? `<span class="post-mod-flag" title="${this.escapeHtml(p.moderation_reason || '')}">⚠️ Kiểm duyệt</span>` : ''}</div>
              <div class="post-time">🕒 ${timeDisplay} <span class="post-visibility-badge" title="Chế độ hiển thị">${visIcon}</span></div>
            </div>
          </div>
          <div class="post-more-wrapper">
            <button class="post-more-btn" onclick="app.togglePostMenu(event, ${app.jsArg(p.id)})" title="Tùy chọn">•••</button>
            <div class="post-more-dropdown" id="post-menu-${p.id}">
              ${isAuthor ? `
                ${!isRepost ? `
                  <button class="post-dropdown-item" onclick="app.openEditPostModal(${app.jsArg(p.id)})">
                    <span>✏️</span> Chỉnh sửa bài viết
                  </button>
                ` : ''}
                <button class="post-dropdown-item text-danger" onclick="app.deleteMyPost(${app.jsArg(p.id)})">
                  <span>🗑️</span> Xóa bài đăng
                </button>
              ` : `
                <button class="post-dropdown-item" onclick="app.hidePost(${app.jsArg(p.id)})">
                  <span>👁️</span> Ẩn bài đăng
                </button>
              `}
              <button class="post-dropdown-item" onclick="app.openReportModal('', ${app.jsArg(p.id)})">
                <span>🚩</span> Báo cáo
              </button>
            </div>
          </div>
        </div>

        ${bodyContentHtml}

        <!-- Thống kê: Cụm icon cảm xúc (Hình 1) và Số lượng bình luận -->
        <div class="post-stats">
          <div>${reactionsClusterHtml}</div>
          <div onclick="app.openCommentModal(${app.jsArg(p.id)})" style="cursor: pointer;">
            💬 ${commentCount} bình luận
          </div>
        </div>

        <!-- Các nút tương tác: Thả cảm xúc, Xem bình luận (Mở Modal Hình 2), Đăng lại (Repost), Chia sẻ vào chat -->
        <div class="post-actions">
          <div class="reaction-btn-wrapper">
            <button class="btn-post-action ${myReactionClass}" onclick="app.toggleLike(${app.jsArg(p.id)}, ${app.jsArg(p.my_reaction)})">
              <span>${myReactionIcon}</span>
              <span>${myReactionLabel}</span>
            </button>
            <div class="reaction-popover">
              <span class="reaction-item" onclick="app.submitReaction(${app.jsArg(p.id)}, 'LIKE')" title="Thích">👍</span>
              <span class="reaction-item" onclick="app.submitReaction(${app.jsArg(p.id)}, 'LOVE')" title="Yêu thích">💗</span>
              <span class="reaction-item" onclick="app.submitReaction(${app.jsArg(p.id)}, 'HAHA')" title="Haha">😆</span>
              <span class="reaction-item" onclick="app.submitReaction(${app.jsArg(p.id)}, 'WOW')" title="Wow">😮</span>
              <span class="reaction-item" onclick="app.submitReaction(${app.jsArg(p.id)}, 'SAD')" title="Buồn">😢</span>
              <span class="reaction-item" onclick="app.submitReaction(${app.jsArg(p.id)}, 'ANGRY')" title="Phẫn nộ">😡</span>
            </div>
          </div>

          <!-- Nút Bình luận: Bấm vào mở Modal xem bình luận như Hình 2 -->
          <button class="btn-post-action" onclick="app.openCommentModal(${app.jsArg(p.id)})">
            <span>💬</span>
            <span>Bình luận</span>
          </button>

          <!-- Nút Đăng lại bài viết của người khác lên trang cá nhân -->
          ${!isAuthor ? `
            <button class="btn-post-action btn-repost" onclick="app.openRepostModal(${app.jsArg(p.id)})" title="Đăng lại bài viết này lên trang cá nhân">
              <span>🔁</span>
              <span>Đăng lại</span>
            </button>
          ` : ''}

          <!-- Nút Chia sẻ: Bấm vào mở Modal chia sẻ vào chat bạn bè/nhóm -->
          <button class="btn-post-action" onclick="app.openShareToChatModal(${app.jsArg(p.id)})">
            <span>📨</span>
            <span>Chia sẻ</span>
          </button>
        </div>
      </article>
    `;
  }

  getPostImageUrls(mediaUrl) {
    if (!mediaUrl) return [];
    try {
      const parsed = JSON.parse(mediaUrl);
      if (Array.isArray(parsed)) return parsed.filter(url => typeof url === 'string' && url.length > 0);
    } catch (_) {}
    return [mediaUrl];
  }

  renderPostMedia(mediaType, mediaUrl, wrapperClass = 'post-media-box') {
    if (!mediaUrl) return '';
    if (mediaType === 'IMAGE') {
      const images = this.getPostImageUrls(mediaUrl);
      if (!images.length) return '';
      return `<div class="${wrapperClass} post-media-gallery">${images.map((url, index) => `
        <div class="post-media-image media-viewer-trigger" data-media-viewer="IMAGE" data-media-url="${this.escapeHtml(url)}">
          <img src="${this.escapeHtml(url)}" alt="Ảnh bài viết ${index + 1}">
        </div>`).join('')}</div>`;
    }
    if (mediaType === 'VIDEO') {
      return `<div class="${wrapperClass} media-viewer-trigger" data-media-viewer="VIDEO" data-media-url="${this.escapeHtml(mediaUrl)}"><video src="${this.escapeHtml(mediaUrl)}" controls></video></div>`;
    }
    return '';
  }

  togglePostMenu(event, postId) {
    if (event) {
      event.stopPropagation();
      event.preventDefault();
    }
    const d = document.getElementById(`post-menu-${postId}`);
    if (d) {
      const isShown = d.classList.contains('show');
      document.querySelectorAll('.post-more-dropdown.show').forEach(el => el.classList.remove('show'));
      if (!isShown) d.classList.add('show');
    }
  }

  async deleteMyPost(postId) {
    const _c1 = await this.showConfirm('Xóa bài đăng này?'); if (!_c1) return;
    try {
      const res = await fetch('/api/posts/delete', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ post_id: postId, user_id: this.currentUser.id })
      });
      const data = await res.json();
      if (data.status === 'ok') {
        this.showToast('Đã xóa bài viết!', 'success');
        const card = document.getElementById(`post-${postId}`);
        if (card) card.remove();
      } else {
        this.showToast(data.message || 'Lỗi xóa bài viết!', 'error');
      }
    } catch (e) {
      this.showToast('Lỗi: ' + e.message, 'error');
    }
  }



  // =========================================================================
  // CHỈNH SỬA BÀI VIẾT (EDIT POST)
  // =========================================================================
  injectEditPostModal() {
    if (document.getElementById('editPostModal')) return;
    const modalDiv = document.createElement('div');
    modalDiv.className = 'modal-overlay';
    modalDiv.id = 'editPostModal';
    modalDiv.style.display = 'none';
    modalDiv.innerHTML = `
      <div class="auth-card" style="max-width: 520px; width: 92%;">
        <div class="auth-header" style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 1rem;">
          <h2 style="font-size: 1.25rem; font-weight: 700;">✏️ Chỉnh sửa bài viết</h2>
          <button type="button" class="btn-close-modal" onclick="app.closeEditPostModal()" style="background: none; border: none; font-size: 1.25rem; cursor: pointer; color: var(--text-muted);" title="Đóng">✖</button>
        </div>
        <input type="hidden" id="editPostIdInput">
        <div class="form-group" style="margin-bottom: 0.75rem;">
          <label style="font-weight: 600; font-size: 0.85rem; margin-bottom: 0.35rem; display: block;">Tiêu đề (tùy chọn):</label>
          <input type="text" id="editPostTitleInput" placeholder="Nhập tiêu đề bài viết..." style="width: 100%; padding: 0.6rem 0.8rem; border-radius: 8px; border: 1px solid var(--border-color); background: var(--bg-surface-secondary); color: var(--text-primary); font-size: 0.9rem;">
        </div>
        <div class="form-group" style="margin-bottom: 0.75rem;">
          <label style="font-weight: 600; font-size: 0.85rem; margin-bottom: 0.35rem; display: block;">Nội dung bài viết:</label>
          <textarea id="editPostContentInput" rows="4" placeholder="Bạn đang nghĩ gì?..." style="width: 100%; padding: 0.6rem 0.8rem; border-radius: 8px; border: 1px solid var(--border-color); background: var(--bg-surface-secondary); color: var(--text-primary); font-size: 0.9rem; resize: vertical;"></textarea>
        </div>
        <div class="form-group" style="margin-bottom: 0.75rem;">
          <label style="font-weight: 600; font-size: 0.85rem; margin-bottom: 0.35rem; display: block;">Quyền riêng tư:</label>
          <select id="editPostVisibilitySelect" style="width: 100%; padding: 0.6rem 0.8rem; border-radius: 8px; border: 1px solid var(--border-color); background: var(--bg-surface-secondary); color: var(--text-primary); font-size: 0.9rem;">
            <option value="PUBLIC">🌍 Công khai</option>
            <option value="FRIENDS">👥 Bạn bè</option>
            <option value="PRIVATE">🔒 Chỉ mình tôi</option>
          </select>
        </div>
        <div class="form-group" style="margin-bottom: 1rem;">
          <label style="font-weight: 600; font-size: 0.85rem; margin-bottom: 0.35rem; display: block;">Ảnh / Video đính kèm:</label>
          <div id="editPostMediaPreviewContainer" style="display: none; position: relative; margin-bottom: 0.5rem; max-height: 220px; overflow: hidden; border-radius: 8px; border: 1px solid var(--border-color); text-align: center; background: #000;">
            <img id="editPostImagePreview" src="" alt="Ảnh xem trước" style="max-height: 200px; max-width: 100%; object-fit: contain; display: none; margin: 0 auto;">
            <video id="editPostVideoPreview" src="" controls style="max-height: 200px; max-width: 100%; display: none; margin: 0 auto;"></video>
            <button type="button" class="btn-remove-media" onclick="app.removeEditPostMedia()" style="position: absolute; top: 8px; right: 8px; background: rgba(0,0,0,0.65); color: #fff; border: none; border-radius: 50%; width: 28px; height: 28px; cursor: pointer; display: flex; align-items: center; justify-content: center; font-size: 0.9rem;" title="Gỡ file đính kèm">✖</button>
          </div>
          <div style="display: flex; gap: 0.5rem;">
            <label class="btn-secondary" style="flex: 1; text-align: center; cursor: pointer; padding: 0.5rem; font-size: 0.85rem; border-radius: 8px; display: flex; align-items: center; justify-content: center; gap: 0.3rem;">
              🖼️ Đổi ảnh
              <input type="file" id="editPostImageInput" accept="image/*" style="display: none;" onchange="app.handleEditPostImageAttachment(this)">
            </label>
            <label class="btn-secondary" style="flex: 1; text-align: center; cursor: pointer; padding: 0.5rem; font-size: 0.85rem; border-radius: 8px; display: flex; align-items: center; justify-content: center; gap: 0.3rem;">
              🎥 Đổi video
              <input type="file" id="editPostVideoInput" accept="video/*" style="display: none;" onchange="app.handleEditPostVideoAttachment(this)">
            </label>
          </div>
        </div>
        <div style="display: flex; gap: 0.75rem;">
          <button type="button" class="btn-secondary" style="flex: 1;" onclick="app.closeEditPostModal()">Hủy</button>
          <button type="button" class="btn-primary" style="flex: 1;" onclick="app.submitEditPost()">Lưu thay đổi</button>
        </div>
      </div>
    `;
    document.body.appendChild(modalDiv);
  }

  async openEditPostModal(postId) {
    document.querySelectorAll('.post-more-dropdown.show').forEach(el => el.classList.remove('show'));
    this.injectEditPostModal();

    let post = (this.postsMap && this.postsMap[postId]) ? this.postsMap[postId] : null;
    if (!post) {
      try {
        const res = await fetch(`/api/posts/detail?id=${postId}&viewer_id=${this.currentUser ? this.currentUser.id : ''}`);
        const data = await res.json();
        if (data.status === 'ok' && data.post) {
          post = data.post;
        }
      } catch (e) {
        console.error(e);
      }
    }

    if (!post) {
      this.showToast('Không tìm thấy thông tin bài viết!', 'error');
      return;
    }

    const idInput = document.getElementById('editPostIdInput');
    const titleInput = document.getElementById('editPostTitleInput');
    const contentInput = document.getElementById('editPostContentInput');
    const visSelect = document.getElementById('editPostVisibilitySelect');

    if (idInput) idInput.value = postId;
    if (titleInput) titleInput.value = post.title || '';
    if (contentInput) contentInput.value = post.content || '';
    if (visSelect) visSelect.value = post.visibility || 'PUBLIC';

    this.editingPostMedia = {
      type: post.media_type || 'NONE',
      url: post.media_url || '',
      changed: false
    };

    this.updateEditPostMediaPreview();

    const modal = document.getElementById('editPostModal');
    if (modal) modal.style.display = 'flex';
  }

  updateEditPostMediaPreview() {
    const container = document.getElementById('editPostMediaPreviewContainer');
    const imgPrev = document.getElementById('editPostImagePreview');
    const vidPrev = document.getElementById('editPostVideoPreview');
    if (!container || !imgPrev || !vidPrev) return;

    if (this.editingPostMedia && this.editingPostMedia.type === 'IMAGE' && this.editingPostMedia.url) {
      const images = this.getPostImageUrls(this.editingPostMedia.url);
      imgPrev.innerHTML = images.map((url, index) =>
        `<img src="${this.escapeHtml(url)}" alt="Ảnh xem trước ${index + 1}">`
      ).join('');
      imgPrev.style.display = 'block';
      vidPrev.style.display = 'none';
      container.style.display = 'block';
    } else if (this.editingPostMedia && this.editingPostMedia.type === 'VIDEO' && this.editingPostMedia.url) {
      vidPrev.src = this.editingPostMedia.url;
      vidPrev.style.display = 'block';
      imgPrev.innerHTML = '';
      imgPrev.style.display = 'none';
      container.style.display = 'block';
    } else {
      imgPrev.innerHTML = '';
      vidPrev.src = '';
      imgPrev.style.display = 'none';
      vidPrev.style.display = 'none';
      container.style.display = 'none';
    }
  }

  handleEditPostImageAttachment(input) {
    if (input.files && input.files[0]) {
      const file = input.files[0];
      if (!this.isMediaFileWithinLimit(file, input)) return;
      const reader = new FileReader();
      reader.onload = (e) => {
        this.editingPostMedia = {
          type: 'IMAGE',
          url: e.target.result,
          changed: true
        };
        this.updateEditPostMediaPreview();
      };
      reader.readAsDataURL(file);
    }
  }

  isMediaFileWithinLimit(file, input) {
    const isImage = file.type.startsWith('image/');
    const maxBytes = isImage ? 5 * 1024 * 1024 : 250 * 1024 * 1024;
    if (file.size <= maxBytes) return true;
    this.showToast('Hình ảnh/Video quá nặng, không thể tải', 'error');
    input.value = '';
    return false;
  }

  handleEditPostVideoAttachment(input) {
    if (input.files && input.files[0]) {
      const file = input.files[0];
      if (!this.isMediaFileWithinLimit(file, input)) return;
      const reader = new FileReader();
      reader.onload = (e) => {
        this.editingPostMedia = {
          type: 'VIDEO',
          url: e.target.result,
          changed: true
        };
        this.updateEditPostMediaPreview();
      };
      reader.readAsDataURL(file);
    }
  }

  removeEditPostMedia() {
    this.editingPostMedia = {
      type: 'NONE',
      url: '',
      changed: true
    };
    this.updateEditPostMediaPreview();
    const imgInput = document.getElementById('editPostImageInput');
    if (imgInput) imgInput.value = '';
    const vidInput = document.getElementById('editPostVideoInput');
    if (vidInput) vidInput.value = '';
  }

  closeEditPostModal() {
    const modal = document.getElementById('editPostModal');
    if (modal) modal.style.display = 'none';
    this.editingPostMedia = null;
  }

  async submitEditPost() {
    const postId = document.getElementById('editPostIdInput').value;
    const title = document.getElementById('editPostTitleInput').value.trim();
    const content = document.getElementById('editPostContentInput').value.trim();
    const visibility = document.getElementById('editPostVisibilitySelect').value;

    const mediaChanged = this.editingPostMedia ? this.editingPostMedia.changed : false;
    const mediaType = this.editingPostMedia ? this.editingPostMedia.type : 'NONE';
    const mediaUrl = this.editingPostMedia ? this.editingPostMedia.url : '';

    if (!title && !content && mediaType === 'NONE') {
      this.showToast('Nội dung hoặc tiêu đề bài viết không được để trống!', 'error');
      return;
    }

    try {
      const res = await fetch('/api/posts/update', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          post_id: postId,
          user_id: this.currentUser ? this.currentUser.id : '',
          title: title,
          content: content,
          visibility: visibility,
          update_media: mediaChanged,
          media_type: mediaType,
          media_url: mediaUrl
        })
      });
      const data = await res.json();
      if (data.status === 'ok') {
        this.showToast('Đã cập nhật bài viết thành công!', 'success');
        this.closeEditPostModal();

        // Cập nhật lại cache và giao diện
        if (this.postsMap && this.postsMap[postId]) {
          this.postsMap[postId].title = title;
          this.postsMap[postId].content = content;
          this.postsMap[postId].visibility = visibility;
          if (mediaChanged) {
            this.postsMap[postId].media_type = mediaType;
            this.postsMap[postId].media_url = mediaUrl;
          }
        }

        // Cập nhật lại bài viết tại chỗ (tránh reload làm mất vị trí cuộn hay ghi đè bài viết khác)
        await this.refreshSinglePostCard(postId);
        if (this.activeModalPostId === postId) {
          this.refreshCommentModal();
        }
      } else {
        this.showToast(data.message || 'Lỗi cập nhật bài viết!', 'error');
      }
    } catch (err) {
      this.showToast('Lỗi: ' + err.message, 'error');
    }
  }

  hidePost(postId) {
    const card = document.getElementById(`post-${postId}`);
    if (card) {
      card.style.transition = 'opacity 0.3s ease, transform 0.3s ease';
      card.style.opacity = '0';
      card.style.transform = 'scale(0.96)';
      setTimeout(() => card.remove(), 300);
      this.showToast('Đã ẩn bài viết khỏi bảng tin của bạn', 'info');
    }
  }

  async readMediaFile(file) {
    return new Promise((resolve, reject) => {
      const reader = new FileReader();
      reader.onload = () => resolve(String(reader.result || ''));
      reader.onerror = () => reject(reader.error || new Error('Không thể đọc tệp media.'));
      reader.readAsDataURL(file);
    });
  }

  updatePostMediaPreview() {
    const container = document.getElementById('mediaPreviewContainer');
    const imgPrev = document.getElementById('postImagePreview');
    const vidPrev = document.getElementById('postVideoPreview');
    if (!container || !imgPrev || !vidPrev) return;

    if (this.currentMedia.type === 'IMAGE') {
      const images = this.getPostImageUrls(this.currentMedia.url);
      imgPrev.innerHTML = images.map((url, index) =>
        `<img src="${this.escapeHtml(url)}" alt="Ảnh xem trước ${index + 1}">`
      ).join('');
      imgPrev.style.display = 'grid';
      vidPrev.style.display = 'none';
      vidPrev.removeAttribute('src');
      container.style.display = 'block';
    } else if (this.currentMedia.type === 'VIDEO') {
      imgPrev.innerHTML = '';
      imgPrev.style.display = 'none';
      vidPrev.src = this.currentMedia.url;
      vidPrev.style.display = 'block';
      container.style.display = 'block';
    } else {
      imgPrev.innerHTML = '';
      imgPrev.style.display = 'none';
      vidPrev.removeAttribute('src');
      vidPrev.style.display = 'none';
      container.style.display = 'none';
    }
  }

  async handleImageAttachment(input) {
    const files = Array.from(input.files || []);
    input.value = '';
    if (!files.length) return;
    if (files.some(file => !file.type.startsWith('image/'))) {
      this.showToast('Chỉ được tải lên tệp hình ảnh.', 'error');
      return;
    }
    if (files.some(file => file.size > 10 * 1024 * 1024)) {
      this.showToast('Mỗi hình ảnh không được vượt quá 10 MB.', 'error');
      return;
    }
    const existing = this.currentMedia.type === 'IMAGE'
      ? this.getPostImageUrls(this.currentMedia.url)
      : [];
    if (existing.length + files.length > 3) {
      this.showToast('Mỗi bài viết chỉ được tải lên tối đa 3 hình ảnh.', 'error');
      return;
    }

    try {
      const addedImages = await Promise.all(files.map(file => this.readMediaFile(file)));
      const images = [...existing, ...addedImages];
      this.currentMedia = { type: 'IMAGE', url: images.length === 1 ? images[0] : JSON.stringify(images) };
      this.updatePostMediaPreview();
    } catch (error) {
      console.error('Không thể đọc hình ảnh bài viết:', error);
      this.showToast('Không thể đọc hình ảnh đã chọn.', 'error');
    }
  }

  async handleVideoAttachment(input) {
    const file = input.files?.[0];
    input.value = '';
    if (!file) return;
    if (!file.type.startsWith('video/')) {
      this.showToast('Chỉ được tải lên tệp video.', 'error');
      return;
    }
    if (file.size > 250 * 1024 * 1024) {
      this.showToast('Video không được vượt quá 250 MB.', 'error');
      return;
    }

    try {
      const url = await this.readMediaFile(file);
      this.currentMedia = { type: 'VIDEO', url };
      this.updatePostMediaPreview();
    } catch (error) {
      console.error('Không thể đọc video bài viết:', error);
      this.showToast('Không thể đọc video đã chọn.', 'error');
    }
  }

  clearPostMedia() {
    this.currentMedia = { type: 'NONE', url: '' };
    const imageInput = document.getElementById('postImageFile');
    if (imageInput) imageInput.value = '';
    const videoInput = document.getElementById('postVideoFile');
    if (videoInput) videoInput.value = '';
    this.updatePostMediaPreview();
  }

  async createPost() {
    const titleInput = document.getElementById('postTitleInput');
    const input = document.getElementById('postContentInput');
    const visibilitySelect = document.getElementById('postVisibilitySelect');

    const title = titleInput ? titleInput.value.trim() : '';
    const content = input ? input.value.trim() : '';
    const visibility = visibilitySelect ? visibilitySelect.value : 'PUBLIC';

    if (!title && !content && this.currentMedia.type === 'NONE') {
      this.showToast('Vui lòng nhập nội dung hoặc đính kèm ảnh/video!', 'error');
      return;
    }

    try {
      const res = await fetch('/api/posts/create', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          author_id: this.currentUser.id,
          title: title,
          visibility: visibility,
          content: content,
          media_type: this.currentMedia.type,
          media_url: this.currentMedia.url
        })
      });
      const data = await res.json();

      if (data.status === 'ok') {
        this.showToast('Đã đăng bài viết thành công!', 'success');
        if (titleInput) titleInput.value = '';
        if (input) input.value = '';
        this.clearPostMedia();
        if (data.post_id && document.getElementById('postsList') &&
            !window.location.pathname.includes('profile.html')) {
          this.deferredFeedPostIds.push(data.post_id);
        }
      } else {
        this.showToast(data.message || 'Lỗi đăng bài!', 'error');
      }
    } catch (err) {
      this.showToast('Lỗi: ' + err.message, 'error');
    }
  }

  async toggleLike(postId, currentReaction) {
    const newType = (currentReaction && currentReaction !== 'NONE') ? 'NONE' : 'LOVE';
    await this.submitReaction(postId, newType);
  }

  async submitReaction(postId, type) {
    if (!this.currentUser) return;
    // Chọn lại đúng cảm xúc đang chọn -> bỏ chọn
    const currentPost = this.postsMap && this.postsMap[postId];
    if (type !== 'NONE' && currentPost && currentPost.my_reaction === type) {
      type = 'NONE';
    }
    try {
      const res = await fetch('/api/posts/reaction', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ post_id: postId, user_id: this.currentUser.id, type: type })
      });
      const data = await res.json();
      if (data.status === 'ok') {
        // Cập nhật lại bài viết tại chỗ
        if (this.activeModalPostId === postId) {
          this.refreshCommentModal();
        }
        await this.refreshSinglePostCard(postId);
      }
    } catch (err) {
      console.error(err);
    }
  }

  // =========================================================================
  // 5. MODAL XEM BÌNH LUẬN (HÌNH 2) & PHẢN HỒI LỒNG NHAU (HÌNH 5)
  // =========================================================================
  async openCommentModal(postId) {
    this.activeModalPostId = postId;
    this.cancelReplyComment();
    const modal = this.ensureCommentModal();

    try {
      const res = await fetch(`/api/posts/detail?id=${encodeURIComponent(postId)}&current_user_id=${this.currentUser ? this.currentUser.id : ''}`);
      const data = await res.json();
      if (data.status === 'ok') {
        this.renderCommentModal(data.post);
        modal.style.display = 'flex';
      } else {
        this.showToast(data.message || 'Không thể xem bình luận!', 'error');
      }
    } catch (err) {
      this.showToast('Lỗi: ' + err.message, 'error');
    }
  }

  closeCommentModal() {
    const modal = document.getElementById('commentModal');
    if (modal) modal.style.display = 'none';
    this.activeModalPostId = null;
    this.cancelReplyComment();
  }

  async refreshCommentModal() {
    if (!this.activeModalPostId) return;
    try {
      const res = await fetch(`/api/posts/detail?id=${encodeURIComponent(this.activeModalPostId)}&current_user_id=${this.currentUser ? this.currentUser.id : ''}`);
      const data = await res.json();
      if (data.status === 'ok') {
        this.renderCommentModal(data.post);
      }
    } catch (e) {}
  }

  renderCommentModal(post) {
    if (!this.postsMap) this.postsMap = {};
    this.postsMap[post.id] = post;
    const modalTitle = document.getElementById('commentModalTitle');
    if (modalTitle) modalTitle.textContent = `Bài viết của ${post.author_name}`;

    const authorAvatar = document.getElementById('cmtPostAuthorAvatar');
    if (authorAvatar) authorAvatar.src = post.author_avatar || '/img/default-avatar.svg';

    const authorName = document.getElementById('cmtPostAuthorName');
    if (authorName) authorName.textContent = post.author_name;

    const postTime = document.getElementById('cmtPostTime');
    if (postTime) postTime.textContent = '🕒 ' + this.formatTimeAgo(post.created_at);

    const postContent = document.getElementById('cmtPostContent');
    if (postContent) postContent.textContent = post.content;

    const mediaContainer = document.getElementById('cmtPostMedia');
    if (mediaContainer) {
      mediaContainer.innerHTML = this.renderPostMedia(post.media_type, post.media_url, 'post-media-box');
    }

    // Reactions cluster
    // Reactions cluster trong Modal
    const clusterBox = document.getElementById('cmtModalReactionsCluster');
    if (clusterBox) {
      const modalTotal = post.total_reactions || (post.reactions ? post.reactions.total : 0) || 0;
      if (modalTotal > 0 && post.top_reactions && post.top_reactions.length > 0) {
        const top3 = post.top_reactions.slice(0, 3);
        const icons = top3.map(r => {
          let emoji = r.icon || '👍';
          if (!r.icon) {
            if (r.type === 'LOVE') emoji = '💗';
            else if (r.type === 'HAHA') emoji = '😆';
            else if (r.type === 'WOW') emoji = '😮';
            else if (r.type === 'SAD') emoji = '😢';
            else if (r.type === 'ANGRY') emoji = '😡';
          }
          return `<span class="reaction-cluster-icon">${emoji}</span>`;
        }).join('');

        let allNames = [];
        top3.forEach(r => {
          const uList = r.user_names || r.users || [];
          if (Array.isArray(uList)) {
            uList.forEach(u => { if (!allNames.includes(u)) allNames.push(u); });
          }
        });
        const tt = allNames.length > 0 ? (allNames.slice(0, 4).join(', ') + (allNames.length > 4 ? ` và ${allNames.length - 4} người khác` : '')) : `${modalTotal} người đã bày tỏ cảm xúc`;

        clusterBox.innerHTML = `
          <div class="reaction-cluster-box" onclick="app.openPostReactionsModal(${app.jsArg(post.id)})" title="${this.escapeHtml(tt)}">
            <div class="reaction-cluster-icons">${icons}</div>
            <span>${modalTotal}</span>
            <div class="reaction-names-tooltip">${this.escapeHtml(tt)}</div>
          </div>
        `;
      } else {
        clusterBox.innerHTML = `<span>0 lượt thích</span>`;
      }
    }

    const commentsCount = document.getElementById('cmtModalCommentsCount');
    if (commentsCount) {
      commentsCount.textContent = Number.isFinite(post.comments_count)
        ? post.comments_count
        : ((post.comments && Array.isArray(post.comments)) ? post.comments.length : 0);
    }

    // Render danh sách bình luận (Hình 2 & 5: Lồng nhau, @tag màu xanh in đậm)
    const listContainer = document.getElementById('cmtModalListContainer');
    if (listContainer) {
      if (!post.comments || post.comments.length === 0) {
        listContainer.innerHTML = '<div style="text-align: center; color: var(--text-muted); font-size: 0.85rem; padding: 1.5rem;">Chưa có bình luận nào. Hãy là người đầu tiên bình luận!</div>';
      } else {
        listContainer.innerHTML = post.comments.map(c => this.renderCommentTreeHtml(post, c)).join('');
      }
    }

    const myAvatar = document.getElementById('cmtMyAvatar');
    if (myAvatar && this.currentUser) myAvatar.src = this.currentUser.avatar || '/img/default-avatar.svg';
  }

  ensureCommentModal() {
    let modal = document.getElementById('commentModal');
    if (modal) return modal;

    const wrapper = document.createElement('div');
    wrapper.innerHTML = `
      <div class="modal-overlay" id="commentModal" style="display:none;" onclick="if(event.target === this) app.closeCommentModal()">
        <div class="comment-modal-dialog">
          <div class="comment-modal-header">
            <span id="commentModalTitle">Bài viết của...</span>
            <button class="btn-icon" onclick="app.closeCommentModal()">✕</button>
          </div>
          <div class="comment-modal-body" id="commentModalBody">
            <div class="comment-modal-post-author">
              <img id="cmtPostAuthorAvatar" src="/img/default-avatar.svg" alt="Avatar">
              <div>
                <div style="font-weight:700;font-size:.95rem" id="cmtPostAuthorName">--</div>
                <div style="font-size:.78rem;color:var(--text-muted)" id="cmtPostTime">--</div>
              </div>
            </div>
            <div class="comment-modal-post-content" id="cmtPostContent"></div>
            <div class="comment-modal-post-media" id="cmtPostMedia"></div>
            <div class="comment-modal-stats">
              <div id="cmtModalReactionsCluster"></div>
              <div><span id="cmtModalCommentsCount">0</span> bình luận</div>
            </div>
            <div class="comment-modal-actions" id="cmtModalActionsBar"></div>
            <div class="comment-list-container" id="cmtModalListContainer"></div>
          </div>
          <div class="comment-modal-footer">
            <div class="replying-indicator" id="replyingIndicator" style="display:none">
              <span>Đang trả lời: <strong id="replyingTargetName">@nguoidung</strong></span>
              <button onclick="app.cancelReplyComment()">✕</button>
            </div>
            <div class="comment-input-row">
              <img id="cmtMyAvatar" src="/img/default-avatar.svg" alt="My avatar">
              <input type="text" id="modalCommentInput" placeholder="Viết bình luận công khai..." onkeypress="if(event.key === 'Enter') app.submitCommentFromModal()">
              <button type="button" title="GIF" onclick="app._commentGifPostId=app.activeModalPostId; app.openGifPicker('comment')" style="border:none;background:transparent;cursor:pointer;font-size:0.85rem;font-weight:700;color:#0084ff;">GIF</button>
              <button onclick="app.submitCommentFromModal()">➤</button>
            </div>
          </div>
        </div>
      </div>
    `;
    modal = wrapper.firstElementChild;
    if (!modal) throw new Error('Không thể khởi tạo khung xem bình luận.');
    document.body.appendChild(modal);
    return modal;
  }

  renderCommentReactionControls(comment) {
    const reactionTypes = [
      ['LIKE', '👍', 'Thích'], ['LOVE', '❤️', 'Yêu thích'], ['HAHA', '😆', 'Haha'],
      ['WOW', '😮', 'Wow'], ['SAD', '😢', 'Buồn'], ['ANGRY', '😡', 'Phẫn nộ']
    ];
    const selected = reactionTypes.find(([type]) => type === comment.my_reaction);
    const triggerIcon = selected ? selected[1] : '👍';
    const triggerLabel = selected ? selected[2] : 'Thích';
    const picker = reactionTypes.map(([type, icon, label]) =>
      `<button type="button" class="comment-reaction-option" title="${label}" aria-label="${label}" onclick="app.submitCommentReaction(${app.jsArg(comment.id)}, ${app.jsArg(type)})">${icon}</button>`
    ).join('');

    let summary = '';
    const reactions = Array.isArray(comment.top_reactions) ? comment.top_reactions : [];
    if (comment.total_reactions > 0) {
      const top = reactions.slice(0, 3);
      summary = `
        <button type="button" class="comment-reaction-summary" onclick="app.openCommentReactionsModal(${app.jsArg(comment.id)})" title="Xem ai đã bày tỏ cảm xúc">
          <span class="comment-reaction-icons">${top.map(reaction =>
            `<span>${this.escapeHtml(reaction.icon || '👍')}</span>`
          ).join('')}</span>
          <span>${comment.total_reactions}</span>
        </button>
      `;
    }

    return `
      <span class="comment-reaction-wrap">
        <button type="button" class="comment-action-btn comment-reaction-trigger${selected ? ' reacted' : ''}" onclick="app.submitCommentReaction(${app.jsArg(comment.id)}, '${selected ? selected[0] : 'LIKE'}')" title="${triggerLabel}">${triggerIcon}</button>
        <span class="comment-reaction-picker">${picker}</span>
      </span>
      ${summary}
    `;
  }

  formatCommentContentHtml(raw) {
    const text = String(raw || '');
    // Hỗ trợ __GIF__:url hoặc URL gif trực tiếp
    const gifMatch = text.match(/__GIF__:(https?:\/\/\S+)/);
    if (gifMatch) {
      const url = this.escapeHtml(gifMatch[1]);
      const rest = this.escapeHtml(text.replace(gifMatch[0], '').trim());
      return `${rest ? `<div>${rest}</div>` : ''}<img class="comment-gif" src="${url}" alt="GIF" loading="lazy" style="max-width:220px;border-radius:10px;margin-top:4px;display:block;">`;
    }
    if (/^https?:\/\/\S+\.(gif)(\?\S*)?$/i.test(text.trim()) || /giphy\.com|tenor\.com/i.test(text)) {
      return `<img class="comment-gif" src="${this.escapeHtml(text.trim())}" alt="GIF" loading="lazy" style="max-width:220px;border-radius:10px;display:block;">`;
    }
    return this.escapeHtml(text);
  }

  renderCommentTreeHtml(post, c) {
    const isPostOwner = (this.currentUser && this.currentUser.id === post.author_id);
    const isCommentAuthor = (this.currentUser && this.currentUser.id === c.author_id);
    const canDelete = isPostOwner || isCommentAuthor;

    const timeAgoStr = this.formatTimeAgo(c.created_at);

    // Render các câu trả lời con (Hình 5)
    let repliesHtml = '';
    if (c.replies && c.replies.length > 0) {
      repliesHtml = `
        <div class="comment-replies-container">
          ${c.replies.map(r => {
            const isReplyAuthor = (this.currentUser && this.currentUser.id === r.author_id);
            const canDeleteReply = isPostOwner || isReplyAuthor;
            const replyTime = this.formatTimeAgo(r.created_at);

            return `
              <div class="comment-item-wrap" id="cmt-${r.id}">
                <div class="comment-item">
                  <img class="c-avatar" src="${app.escapeHtml(r.author_avatar || '/img/default-avatar.svg')}" onclick="app.openUserProfile(${app.jsArg(r.author_token || r.author_id)})" style="cursor: pointer;">
                  <div class="comment-bubble">
                    <div class="c-author" onclick="app.openUserProfile(${app.jsArg(r.author_token || r.author_id)})">${this.escapeHtml(r.author_name)}</div>
                    <div class="c-text">
                      ${r.reply_to_author ? `<span class="mention-user-tag" onclick="app.openUserProfile(${app.jsArg(r.author_token || r.author_id)})">@${this.escapeHtml(r.reply_to_author)}</span>` : ''}
                      ${this.formatCommentContentHtml(r.content)}
                    </div>
                  </div>
                  <div class="comment-more-wrapper">
                    <button class="comment-more-btn" onclick="app.toggleCommentMenu(event, ${app.jsArg(r.id)})" title="Tùy chọn">•••</button>
                    <div class="comment-more-dropdown" id="cmt-menu-${r.id}">
                      ${canDeleteReply ? `
                        <button class="post-dropdown-item text-danger" onclick="app.deleteComment(${app.jsArg(r.id)})">
                          <span>🗑️</span> Xóa bình luận
                        </button>
                      ` : ''}
                      <button class="post-dropdown-item" onclick="app.openReportModal('', '', ${app.jsArg(r.id)})">
                        <span>🚩</span> Báo cáo
                      </button>
                    </div>
                  </div>
                </div>
                <div class="comment-actions-bar">
                  <span>${replyTime}</span>
                  ${this.renderCommentReactionControls(r)}
                  <button class="comment-action-btn" onclick="app.prepareReplyComment(${app.jsArg(c.id)}, ${app.jsArg(r.author_name)})">Phản hồi</button>
                </div>
              </div>
            `;
          }).join('')}
        </div>
      `;
    }

    return `
      <div class="comment-item-wrap" id="cmt-${c.id}">
        <div class="comment-item">
          <img class="c-avatar" src="${app.escapeHtml(c.author_avatar || '/img/default-avatar.svg')}" onclick="app.openUserProfile(${app.jsArg(c.author_token || c.author_id)})" style="cursor: pointer;">
          <div class="comment-bubble">
            <div class="c-author" onclick="app.openUserProfile(${app.jsArg(c.author_token || c.author_id)})">${this.escapeHtml(c.author_name)}</div>
            <div class="c-text">${this.formatCommentContentHtml(c.content)}</div>
          </div>
          <div class="comment-more-wrapper">
            <button class="comment-more-btn" onclick="app.toggleCommentMenu(event, ${app.jsArg(c.id)})" title="Tùy chọn">•••</button>
            <div class="comment-more-dropdown" id="cmt-menu-${c.id}">
              ${canDelete ? `
                <button class="post-dropdown-item text-danger" onclick="app.deleteComment(${app.jsArg(c.id)})">
                  <span>🗑️</span> Xóa bình luận
                </button>
              ` : ''}
              <button class="post-dropdown-item" onclick="app.openReportModal('', '', ${app.jsArg(c.id)})">
                <span>🚩</span> Báo cáo
              </button>
            </div>
          </div>
        </div>
        <div class="comment-actions-bar">
          <span>${timeAgoStr}</span>
          ${this.renderCommentReactionControls(c)}
          <button class="comment-action-btn" onclick="app.prepareReplyComment(${app.jsArg(c.id)}, ${app.jsArg(c.author_name)})">Phản hồi</button>
        </div>
        ${repliesHtml}
      </div>
    `;
  }

  async submitCommentReaction(commentId, type) {
    if (!this.currentUser) {
      this.showToast('Vui lòng đăng nhập để bày tỏ cảm xúc!', 'error');
      return;
    }
    const comment = this.findCommentInPost(this.activeModalPostId, commentId);
    const reactionType = comment && comment.my_reaction === type ? 'NONE' : type;
    try {
      const res = await fetch('/api/comments/reaction', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ comment_id: commentId, user_id: this.currentUser.id, type: reactionType })
      });
      const data = await res.json();
      if (data.status !== 'ok') {
        this.showToast(data.message || 'Không thể cập nhật cảm xúc bình luận!', 'error');
        return;
      }
      await this.refreshCommentModal();
    } catch (error) {
      console.error('Lỗi thả cảm xúc bình luận:', error);
      this.showToast('Không thể cập nhật cảm xúc bình luận!', 'error');
    }
  }

  findCommentInPost(postId, commentId) {
    const post = this.postsMap && this.postsMap[postId];
    if (!post || !Array.isArray(post.comments)) return null;
    const findIn = comments => {
      for (const comment of comments) {
        if (String(comment.id) === String(commentId)) return comment;
        const reply = findIn(comment.replies || []);
        if (reply) return reply;
      }
      return null;
    };
    return findIn(post.comments);
  }

  toggleCommentMenu(event, commentId) {
    if (event) {
      event.stopPropagation();
      event.preventDefault();
    }
    const d = document.getElementById(`cmt-menu-${commentId}`);
    if (d) {
      const isShown = d.classList.contains('show');
      document.querySelectorAll('.comment-more-dropdown.show').forEach(el => el.classList.remove('show'));
      if (!isShown) d.classList.add('show');
    }
  }

  prepareReplyComment(commentId, authorName) {
    this.replyingToCommentId = commentId;
    this.replyingToAuthorName = authorName;

    const indicator = document.getElementById('replyingIndicator');
    const label = document.getElementById('replyingTargetName');
    if (indicator && label) {
      label.textContent = `@${authorName}`;
      indicator.style.display = 'flex';
    }

    const input = document.getElementById('modalCommentInput');
    if (input) {
      input.focus();
      input.placeholder = `Trả lời ${authorName}...`;
    }
  }

  cancelReplyComment() {
    this.replyingToCommentId = null;
    this.replyingToAuthorName = null;
    const indicator = document.getElementById('replyingIndicator');
    if (indicator) indicator.style.display = 'none';
    const input = document.getElementById('modalCommentInput');
    if (input) input.placeholder = 'Viết bình luận công khai...';
  }

  async submitCommentFromModal() {
    const input = document.getElementById('modalCommentInput');
    if (!input || !this.activeModalPostId) return;
    const text = input.value.trim();
    if (!text) return;

    try {
      const res = await fetch('/api/posts/comment', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          post_id: this.activeModalPostId,
          author_id: this.currentUser.id,
          content: text,
          reply_to_id: this.replyingToCommentId || '',
          reply_to_author: this.replyingToAuthorName || ''
        })
      });
      const data = await res.json();
      if (data.status === 'ok') {
        input.value = '';
        this.cancelReplyComment();
        this.refreshCommentModal();
        await this.refreshSinglePostCard(this.activeModalPostId);
      } else {
        this.showToast(data.message || 'Lỗi gửi bình luận!', 'error');
      }
    } catch (err) {
      this.showToast('Lỗi: ' + err.message, 'error');
    }
  }

  async deleteComment(commentId) {
    const _c2 = await this.showConfirm('Xóa bình luận này?'); if (!_c2) return;
    try {
      const res = await fetch('/api/comments/delete', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ comment_id: commentId, user_id: this.currentUser.id })
      });
      const data = await res.json();
      if (data.status === 'ok') {
        this.showToast('Đã xóa bình luận!', 'success');
        this.refreshCommentModal();
        await this.refreshSinglePostCard(this.activeModalPostId);
      } else {
        this.showToast(data.message || 'Không thể xóa bình luận!', 'error');
      }
    } catch (err) {
      this.showToast('Lỗi: ' + err.message, 'error');
    }
  }

  // =========================================================================
  // 6. GỢI Ý TÌM KIẾM THỜI GIAN THỰC & LỊCH SỬ TÌM KIẾM
  // =========================================================================
  async showSearchHistory() {
    if (!this.currentUser) return;
    const searchInput = document.getElementById('searchInput');
    const dropdown = document.getElementById('searchSuggestionsDropdown');
    if (!searchInput || !dropdown) return;

    if (searchInput.value.trim() !== '') {
      return;
    }

    try {
      const res = await fetch(`/api/search/history?user_id=${encodeURIComponent(this.currentUser.id)}`);
      const data = await res.json();
      if (data.status === 'ok') {
        const history = data.history || [];
        let html = `
          <div class="search-history-box">
            <div class="search-history-header">
              <span>🕒 Lịch sử tìm kiếm gần đây</span>
              ${history.length > 0 ? `<button class="search-history-clear-btn" onclick="app.clearAllSearchHistory(event)">Xóa tất cả</button>` : ''}
            </div>
            <div class="search-history-list">
        `;
        if (history.length === 0) {
          html += `<div class="search-history-empty">Chưa có lịch sử tìm kiếm</div>`;
        } else {
          html += history.map(item => {
            const safeQ = (item.query || '').replace(/\\/g, '\\\\').replace(/'/g, "\\'").replace(/"/g, '&quot;');
            return `
            <div class="search-history-item" onclick="app.reSearchHistory(${app.jsArg(safeQ)})">
              <div class="search-history-item-left">
                <span class="search-history-icon">🕒</span>
                <span class="search-history-text">${this.escapeHtml(item.query)}</span>
              </div>
              <button class="search-history-del-btn" onclick="app.deleteSearchHistoryItem(${item.id}, event)" title="Xóa khỏi lịch sử">✕</button>
            </div>`;
          }).join('');
        }
        html += `</div></div>`;
        dropdown.innerHTML = html;
        dropdown.style.display = 'flex';
      }
    } catch (e) {
      console.error(e);
    }
  }

  async reSearchHistory(query) {
    const searchInput = document.getElementById('searchInput');
    if (searchInput) searchInput.value = query;
    const dropdown = document.getElementById('searchSuggestionsDropdown');
    if (dropdown) dropdown.style.display = 'none';
    this.navigateToSearchPage(query);
  }

  async deleteSearchHistoryItem(id, event) {
    if (event) {
      event.preventDefault();
      event.stopPropagation();
    }
    if (!this.currentUser) return;
    try {
      const res = await fetch(`/api/search/history?user_id=${encodeURIComponent(this.currentUser.id)}&id=${id}`, {
        method: 'DELETE'
      });
      const data = await res.json();
      if (data.status === 'ok') {
        this.showSearchHistory();
      }
    } catch (e) {
      console.error(e);
    }
  }

  async clearAllSearchHistory(event) {
    if (event) {
      event.preventDefault();
      event.stopPropagation();
    }
    if (!this.currentUser) return;
    try {
      const res = await fetch(`/api/search/history?user_id=${encodeURIComponent(this.currentUser.id)}&all=true`, {
        method: 'DELETE'
      });
      const data = await res.json();
      if (data.status === 'ok') {
        this.showSearchHistory();
      }
    } catch (e) {
      console.error(e);
    }
  }

  async saveSearchHistory(query) {
    if (!this.currentUser || !query || !query.trim()) return;
    try {
      await fetch('/api/search/history', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          user_id: this.currentUser.id,
          query: query.trim()
        })
      });
    } catch (e) {}
  }

  handleSearchInput(val) {
    clearTimeout(this.searchDebounceTimer);
    const dropdown = document.getElementById('searchSuggestionsDropdown');
    if (!dropdown) return;

    const q = val.trim();
    if (!q) {
      this.showSearchHistory();
      return;
    }

    this.searchDebounceTimer = setTimeout(async () => {
      try {
        const res = await fetch(`/api/search?q=${encodeURIComponent(q)}&filter=ALL&user_id=${this.currentUser ? this.currentUser.id : ''}`);
        const data = await res.json();
        if (data.status === 'ok') {
          this.renderSearchSuggestionsDropdown(data, q);
        }
      } catch (err) {
        console.error(err);
      }
    }, 200);
  }

  renderSearchSuggestionsDropdown(data, query) {
    const dropdown = document.getElementById('searchSuggestionsDropdown');
    if (!dropdown) return;

    const hasUsers = data.users && data.users.length > 0;
    const hasGroups = data.groups && data.groups.length > 0;

    if (!hasUsers && !hasGroups) {
      dropdown.innerHTML = `<div style="padding: 0.8rem; text-align: center; font-size: 0.85rem; color: var(--text-muted);">Không tìm thấy kết quả phù hợp cho "${this.escapeHtml(query)}"</div>`;
      dropdown.style.display = 'block';
      return;
    }

    let html = '';
    if (hasUsers) {
      html += `<div class="search-suggestion-section-title">Mọi người</div>`;
      html += data.users.slice(0, 5).map(u => `
        <div class="search-suggestion-item" onclick="app.saveSearchHistory(${app.jsArg(encodeURIComponent(query))}); app.openUserProfile(${app.jsArg(u.user_token || u.id)})">
          <img src="${app.escapeHtml(u.avatar || '/img/default-avatar.svg')}" alt="Avatar">
          <div class="search-suggestion-info">
            <span class="ss-name">${this.escapeHtml(u.name)}</span>
            <span class="ss-sub">${this.escapeHtml(u.subtitle || ('@' + u.username))}</span>
          </div>
        </div>
      `).join('');
    }

    if (hasGroups) {
      html += `<div class="search-suggestion-section-title">Nhóm</div>`;
      html += data.groups.slice(0, 3).map(g => `
        <div class="search-suggestion-item" onclick="app.saveSearchHistory(${app.jsArg(encodeURIComponent(query))}); app.openGroupChat(${app.jsArg(g.id)}, ${app.jsArg(g.name)}, ${app.jsArg(g.avatar || '')})">
          <img src="${app.escapeHtml(g.avatar || '/img/default-avatar.svg')}" alt="Avatar">
          <div class="search-suggestion-info">
            <span class="ss-name">${this.escapeHtml(g.name)}</span>
            <span class="ss-sub">${this.escapeHtml(g.subtitle || '')}</span>
          </div>
        </div>
      `).join('');
    }

    html += `
      <div class="search-suggestion-more" onclick="app.navigateToSearchPage(${app.jsArg(encodeURIComponent(query))})">
        🔍 Nhấn Enter hoặc nhấn vào đây để xem toàn bộ kết quả cho "${this.escapeHtml(query)}"
      </div>
    `;

    dropdown.innerHTML = html;
    dropdown.style.display = 'flex';
  }

  navigateToSearchPage(query) {
    if (!query) return;
    this.saveSearchHistory(query);
    window.location.href = `search.html?q=${encodeURIComponent(query)}&filter=ALL`;
  }

  // =========================================================================
  // 7. TRANG TÌM KIẾM RIÊNG CÓ BỘ LỌC (search.html - HÌNH 4)
  // =========================================================================
  initSearchPage(query, activeFilter = 'ALL') {
    if (query) {
      this.saveSearchHistory(query);
    }
    this.currentSearchQuery = query;
    this.currentSearchFilter = activeFilter;

    const label = document.getElementById('searchQueryLabel');
    if (label) label.textContent = `"${query}"`;

    const searchInput = document.getElementById('searchInput');
    if (searchInput) searchInput.value = query;

    this.changeSearchFilter(activeFilter);
  }

  changeSearchFilter(filter) {
    this.currentSearchFilter = filter;
    ['btnFilterAll', 'btnFilterPeople', 'btnFilterGroups', 'btnFilterPosts'].forEach(id => {
      const btn = document.getElementById(id);
      if (btn) btn.classList.remove('active');
    });

    if (filter === 'ALL') document.getElementById('btnFilterAll')?.classList.add('active');
    else if (filter === 'PEOPLE') document.getElementById('btnFilterPeople')?.classList.add('active');
    else if (filter === 'GROUPS') document.getElementById('btnFilterGroups')?.classList.add('active');
    else if (filter === 'POSTS') document.getElementById('btnFilterPosts')?.classList.add('active');

    this.fetchAndRenderSearchResults();
  }


  renderSearchUserCard(u) {
    const id = u.id || '';
    const token = u.user_token || id;
    const name = this.escapeHtml(u.name || 'Người dùng');
    const avatar = this.escapeHtml(u.avatar || '/img/default-avatar.svg');
    const username = this.escapeHtml(u.username || '');
    const subtitle = this.escapeHtml(u.subtitle || (username ? '@' + u.username : ''));
    const bio = this.escapeHtml(u.bio || '');
    const isFriend = !!u.is_friend;
    const req = (u.request_status || 'NONE').toUpperCase();
    const isSelf = this.currentUser && this.currentUser.id === id;
    const safeName = String(u.name || 'Người dùng').replace(/\\/g, '\\\\').replace(/'/g, "\\'");

    let relationBtn = '';
    if (isSelf) {
      relationBtn = `<span class="suc-friend-badge">Đây là bạn</span>`;
    } else if (isFriend) {
      relationBtn = `<span class="suc-friend-badge">✓ Bạn bè</span>`;
    } else if (req === 'SENT') {
      relationBtn = `<button class="btn-secondary btn-pending-status" type="button" onclick="app.handleCancelFriendRequest(${app.jsArg(id)}, ${app.jsArg(safeName)})"><span class="btn-pending-label">Đã gửi lời mời kết bạn</span><span class="btn-cancel-request-label">Hủy lời mời kết bạn</span></button>`;
    } else if (req === 'INCOMING') {
      relationBtn = `<button class="btn-primary" type="button" onclick="app.respondFriendRequest(${app.jsArg(id)}, 'ACCEPT')">Chấp nhận kết bạn</button>`;
    } else {
      relationBtn = `<button class="btn-primary" type="button" onclick="app.sendFriendRequest(${app.jsArg(id)}, ${app.jsArg(safeName)})">+ Thêm bạn bè</button>`;
    }

    const messageBtn = (!isSelf)
      ? `<button class="btn-secondary" type="button" onclick="app.openChatWith(${app.jsArg(id)}, ${app.jsArg(safeName)}, ${app.jsArg(avatar)}, ${app.jsArg(token)})">💬 Nhắn tin</button>`
      : '';

    return `
      <div class="search-user-card" data-user-id="${this.escapeHtml(id)}">
        <div class="suc-avatar-wrap">
          <img class="suc-avatar" src="${avatar}" alt="${name}" onclick="app.openUserProfile(${app.jsArg(token)})" onerror="this.src='/img/default-avatar.svg'">
          ${isSelf ? '' : `
          <button type="button" class="suc-menu-btn" title="Tùy chọn" onclick="app.toggleSearchUserMenu(event, ${app.jsArg(id)})">⋯</button>
          <div class="suc-menu" id="searchUserMenu-${this.escapeHtml(id)}">
            <button type="button" onclick="app.openUserProfile(${app.jsArg(token)}); app.closeAllSearchUserMenus();">👤 Xem trang cá nhân</button>
            <button type="button" class="danger" onclick="app.openReportModal(${app.jsArg(id)}, ''); app.closeAllSearchUserMenus();">🚩 Báo cáo</button>
            <button type="button" class="danger" onclick="app.blockUserFromSearch(${app.jsArg(id)}, ${app.jsArg(safeName)});">🚫 Chặn</button>
          </div>`}
        </div>
        <div class="suc-body">
          <h4 class="suc-name" onclick="app.openUserProfile(${app.jsArg(token)})">${name}</h4>
          <div class="suc-meta">${subtitle || (username ? '@' + username : '')}</div>
          <div class="suc-bio" onclick="app.openUserProfile(${app.jsArg(token)})" style="cursor:pointer;">${bio || 'Chưa có giới thiệu.'}</div>
          <div class="suc-actions">
            <div id="search-friend-btn-${this.escapeHtml(id)}" class="suc-friend-btn-box">${relationBtn}</div>
            ${messageBtn}
          </div>
        </div>
      </div>`;
  }

  toggleSearchUserMenu(event, userId) {
    if (event) {
      event.preventDefault();
      event.stopPropagation();
    }
    const menu = document.getElementById('searchUserMenu-' + userId);
    if (!menu) return;
    const willOpen = !menu.classList.contains('open');
    this.closeAllSearchUserMenus();
    if (willOpen) menu.classList.add('open');
  }

  closeAllSearchUserMenus() {
    document.querySelectorAll('.search-user-card .suc-menu.open').forEach(m => m.classList.remove('open'));
  }


  toggleProfileMoreMenu(event) {
    if (event) { event.preventDefault(); event.stopPropagation(); }
    const menu = document.getElementById('profileMoreMenu');
    if (!menu) return;
    const open = menu.style.display === 'none' || !menu.style.display;
    this.closeProfileMoreMenu();
    if (open) menu.style.display = 'block';
  }

  closeProfileMoreMenu() {
    const menu = document.getElementById('profileMoreMenu');
    if (menu) menu.style.display = 'none';
  }

  async blockUserFromProfile(userId, userName = '') {
    if (!this.currentUser || !userId) return;
    if (this.currentUser.id === userId) return;
    const ok = await this.showConfirm(`Bạn có chắc chắn muốn chặn ${userName || 'người này'} không?`);
    if (!ok) return;
    try {
      const res = await fetch('/api/users/block', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ my_id: this.currentUser.id, target_id: userId })
      });
      const data = await res.json();
      if (data.status === 'ok') {
        this.showToast('Đã chặn người dùng.', 'success');
        // Cập nhật nút trên profile
        const actionRow = document.getElementById('profileActionsRow');
        if (actionRow) {
          actionRow.innerHTML = `
            <button class="btn-secondary" type="button" onclick="app.unblockUser(${app.jsArg(userId)}, ${app.jsArg(userName)})">✓ Đã chặn · Bỏ chặn</button>
            <button class="btn-secondary" style="color:var(--accent-red);" onclick="app.openReportModal(${app.jsArg(userId)}, '')">🚩 Báo cáo</button>
          `;
        }
      } else {
        this.showToast(data.message || 'Không thể chặn.', 'error');
      }
    } catch (e) {
      this.showToast('Lỗi: ' + e.message, 'error');
    }
  }

  openBlockedUsersModal() {
    let modal = document.getElementById('blockedUsersModal');
    if (!modal) {
      modal = document.createElement('div');
      modal.id = 'blockedUsersModal';
      modal.className = 'modal-overlay';
      modal.style.display = 'none';
      modal.innerHTML = `
        <div class="auth-card" style="max-width:480px;width:92%;max-height:80vh;overflow:auto;">
          <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:1rem;">
            <h3 style="margin:0;">Danh sách người đã chặn</h3>
            <button class="btn-icon" type="button" onclick="app.closeBlockedUsersModal()">✕</button>
          </div>
          <p style="font-size:0.85rem;color:var(--text-muted);margin-bottom:0.85rem;">Người bị chặn không thể nhắn tin hoặc gửi lời mời kết bạn cho bạn.</p>
          <div id="blockedUsersModalList" style="display:flex;flex-direction:column;gap:0.65rem;">
            <div style="text-align:center;color:var(--text-muted);padding:1rem;">Đang tải...</div>
          </div>
        </div>`;
      document.body.appendChild(modal);
      modal.addEventListener('click', (e) => { if (e.target === modal) this.closeBlockedUsersModal(); });
    }
    modal.style.display = 'flex';
    this.renderBlockedUsersModalList();
  }

  closeBlockedUsersModal() {
    const modal = document.getElementById('blockedUsersModal');
    if (modal) modal.style.display = 'none';
  }

  async renderBlockedUsersModalList() {
    const list = document.getElementById('blockedUsersModalList');
    if (!list || !this.currentUser) return;
    list.innerHTML = '<div style="text-align:center;color:var(--text-muted);padding:1rem;">Đang tải...</div>';
    try {
      const res = await fetch(`/api/users/blocked?user_id=${encodeURIComponent(this.currentUser.id)}`);
      const data = await res.json();
      const users = data.users || [];
      if (!users.length) {
        list.innerHTML = '<div style="text-align:center;color:var(--text-muted);padding:1.2rem;">Chưa chặn ai.</div>';
        return;
      }
      list.innerHTML = users.map(u => `
        <div style="display:flex;align-items:center;gap:0.75rem;padding:0.65rem;border:1px solid var(--border-color);border-radius:10px;background:var(--bg-surface);">
          <img src="${this.escapeHtml(u.avatar || '/img/default-avatar.svg')}" style="width:48px;height:48px;border-radius:50%;object-fit:cover;background:#d2d6dc;" onerror="this.src='/img/default-avatar.svg'">
          <div style="flex:1;min-width:0;">
            <div style="font-weight:700;">${this.escapeHtml(u.name || u.fullname || 'Người dùng')}</div>
            <div style="font-size:0.8rem;color:var(--text-muted);">@${this.escapeHtml(u.username || '')}</div>
          </div>
          <button class="btn-primary" type="button" style="font-size:0.82rem;"
            onclick="app.unblockUser(${app.jsArg(u.id)}, ${app.jsArg(u.name || u.fullname || '')}).then(()=>app.renderBlockedUsersModalList())">Bỏ chặn</button>
        </div>
      `).join('');
    } catch (e) {
      list.innerHTML = '<div style="color:var(--accent-red);padding:1rem;">Không tải được danh sách.</div>';
    }
  }

  async blockUserFromSearch(userId, userName = '') {
    this.closeAllSearchUserMenus();
    if (!this.currentUser || !userId) return;
    const ok = await this.showConfirm(`Bạn có chắc muốn chặn ${userName || 'người dùng này'}?`);
    if (!ok) return;
    try {
      const res = await fetch('/api/users/block', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ my_id: this.currentUser.id, target_id: userId })
      });
      const data = await res.json();
      if (data.status === 'ok') {
        this.showToast('Đã chặn người dùng.', 'success');
        this.fetchAndRenderSearchResults();
      } else {
        this.showToast(data.message || 'Không thể chặn.', 'error');
      }
    } catch (e) {
      this.showToast('Lỗi: ' + e.message, 'error');
    }
  }

  async fetchAndRenderSearchResults() {
    const list = document.getElementById('searchResultsList');
    if (!list) return;

    list.innerHTML = '<div style="text-align: center; padding: 2rem; color: var(--text-muted);">Đang tìm kiếm dữ liệu...</div>';

    try {
      const res = await fetch(`/api/search?q=${encodeURIComponent(this.currentSearchQuery)}&filter=${this.currentSearchFilter}&user_id=${this.currentUser ? this.currentUser.id : ''}`);
      const data = await res.json();

      if (data.status === 'ok') {
        let html = '';
        const users = data.users || [];
        const groups = data.groups || [];
        const posts = data.posts || [];

        if (users.length === 0 && groups.length === 0 && posts.length === 0) {
          list.innerHTML = '<div style="text-align: center; padding: 2rem; color: var(--text-muted);">Không tìm thấy kết quả nào.</div>';
          return;
        }

        if (users.length > 0) {
          html += `
            <div class="search-filter-card" style="margin-bottom: 1rem;">
              <h3 style="font-size: 1.05rem; margin-bottom: 0.9rem;">Mọi người · ${users.length} kết quả</h3>
              <div class="search-people-grid">
                ${users.map(u => this.renderSearchUserCard(u)).join('')}
              </div>
            </div>
          `;
        }

        if (groups.length > 0) {
          html += `
            <div class="search-filter-card" style="margin-bottom: 1rem;">
              <h3 style="font-size: 1.05rem; margin-bottom: 0.8rem;">Nhóm</h3>
              <div class="friends-grid">
                ${groups.map(g => `
                  <div class="friend-card">
                    <img class="fc-avatar" src="${app.escapeHtml(g.avatar || '/img/default-avatar.svg')}" style="height: 120px;">
                    <div class="friend-card-content">
                      <h4>${this.escapeHtml(g.name)}</h4>
                      <div class="fc-sub">${this.escapeHtml(g.subtitle || '')}</div>
                      <p style="font-size: 0.8rem; color: var(--text-muted);">${this.escapeHtml(g.description || '')}</p>
                      <div class="friend-card-actions">
                        <button class="btn-primary" style="font-size: 0.8rem;" onclick="app.openGroupChat(${app.jsArg(g.id)}, ${app.jsArg(g.name)}, ${app.jsArg(g.avatar || '')})">💬 Vào nhắn tin</button>
                      </div>
                    </div>
                  </div>
                `).join('')}
              </div>
            </div>
          `;
        }

        if (posts.length > 0) {
          html += `
            <div class="search-filter-card">
              <h3 style="font-size: 1.05rem; margin-bottom: 0.8rem;">Bài viết</h3>
              <div class="search-post-results">
                ${posts.map(post => this.renderPostCard(post)).join('')}
              </div>
            </div>
          `;
        }

        list.innerHTML = html;
      }
    } catch (err) {
      list.innerHTML = '<div style="text-align: center; padding: 2rem; color: var(--accent-red);">Lỗi tải kết quả!</div>';
    }
  }

  // =========================================================================
  // 8. THÔNG BÁO HÌNH CÁI CHUÔNG (HÌNH 4)
  // =========================================================================
  async loadNotifications(append = false) {
    if (!this.currentUser) return;
    if (this.notificationsLoading) return;
    if (!append) {
      this.notificationOffset = 0;
      this.notificationHasMore = true;
      this.notificationItems = [];
    }
    if (append && !this.notificationHasMore) return;
    this.notificationsLoading = true;
    try {
      const res = await fetch(`/api/notifications?user_id=${encodeURIComponent(this.currentUser.id)}&offset=${this.notificationOffset}`);
      const data = await res.json();

      if (!res.ok || data.status !== 'ok') throw new Error(data.message || 'Không thể tải thông báo.');
      const badge = document.getElementById('unreadNotifCount');
      if (badge) {
        if (data.unread_count > 0) {
          badge.textContent = data.unread_count;
          badge.style.display = 'inline-block';
        } else {
          badge.style.display = 'none';
        }
      }

      const list = document.getElementById('notifList');
      if (list) {
        if (!append) {
          list.innerHTML = `
            <section class="notif-section">
              <h5 class="notif-section-title">Mới</h5>
              <div class="notif-section-items" data-section="new"></div>
            </section>
            <section class="notif-section">
              <h5 class="notif-section-title">Trước đó</h5>
              <div class="notif-section-items" data-section="previous"></div>
            </section>`;
        }
        const page = Array.isArray(data.notifications) ? data.notifications : [];
        this.notificationItems.push(...page);
        for (const notification of page) {
          const sectionName = notification.is_read ? 'previous' : 'new';
          const section = list.querySelector(`.notif-section-items[data-section="${sectionName}"]`);
          if (section) section.insertAdjacentHTML('beforeend', this.renderNotificationItem(notification));
        }
        list.querySelectorAll('.notif-section').forEach(section => {
          const items = section.querySelector('.notif-section-items');
          section.style.display = items && items.children.length ? '' : 'none';
        });
        this.notificationOffset += page.length;
        this.notificationHasMore = !!data.has_more;
        if (!this.notificationItems.length) {
          list.innerHTML = '<div class="notif-empty">Không có thông báo nào.</div>';
          this.stopObservingInfiniteScrollList(list);
        } else if (this.notificationHasMore) {
          this.observeInfiniteScrollList(list, () => this.loadNotifications(true), list);
        } else {
          this.stopObservingInfiniteScrollList(list);
        }
      }
    } catch (error) {
      console.error('Lỗi tải thông báo:', error);
      this.showToast(error.message || 'Không thể tải thông báo.', 'error');
    } finally {
      this.notificationsLoading = false;
    }
  }

  renderNotificationItem(notification) {
    const id = Number(notification.id) || 0;
    const link = this.escapeHtml(notification.link || '');
    return `
      <div class="notif-item ${notification.is_read ? '' : 'unread'}" id="notif-item-${id}" onclick="app.handleNotificationClick(${app.jsArg(link)}, ${id})">
        <img src="${this.escapeHtml(notification.actor_avatar || '/img/default-avatar.svg')}" alt="Avatar">
        <div class="notif-text">
          <strong>${this.escapeHtml(notification.actor_name || 'Người dùng')}</strong> ${this.escapeHtml(notification.content || '')}
          <div class="notif-time">${this.formatTimeAgo(notification.created_at)}</div>
        </div>
        ${!notification.is_read ? '<span class="unread-blue-dot notif-dot" title="Thông báo chưa đọc"></span>' : ''}
        <button type="button" class="notif-options-button" aria-label="Tùy chọn thông báo" onclick="event.stopPropagation(); app.toggleNotificationOptions(event, ${id})">•••</button>
        <div class="notif-options-menu" id="notif-options-${id}" onclick="event.stopPropagation()">
          ${!notification.is_read ? `<button type="button" onclick="app.markNotificationRead(${id})">Đánh dấu đã đọc</button>` : ''}
          <button type="button" class="danger" onclick="app.deleteNotification(${id})">Xóa thông báo này</button>
        </div>
      </div>`;
  }

  toggleNotificationOptions(event, notifId) {
    document.querySelectorAll('.notif-options-menu.open').forEach(menu => menu.classList.remove('open'));
    const menu = document.getElementById(`notif-options-${notifId}`);
    if (!menu) return;
    menu.classList.toggle('open');
    if (event) event.stopPropagation();
  }

  async markNotificationRead(notifId) {
    if (!this.currentUser) return;
    try {
      const response = await fetch('/api/notifications/read', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ user_id: this.currentUser.id, notification_id: notifId })
      });
      const data = await response.json();
      if (!response.ok || data.status !== 'ok') throw new Error(data.message || 'Không thể cập nhật thông báo.');
      const item = document.getElementById(`notif-item-${notifId}`);
      if (item) {
        item.classList.remove('unread');
        item.querySelector('.unread-blue-dot')?.remove();
        item.querySelector('.notif-options-menu')?.classList.remove('open');
        const previous = document.querySelector('.notif-section-items[data-section="previous"]');
        if (previous) previous.appendChild(item);
      }
      const notification = this.notificationItems.find(entry => Number(entry.id) === notifId);
      if (notification) notification.is_read = 1;
      this.loadNotifications();
    } catch (error) {
      console.error('Lỗi đánh dấu thông báo đã đọc:', error);
      this.showToast(error.message || 'Không thể cập nhật thông báo.', 'error');
    }
  }

  async deleteNotification(notifId) {
    if (!this.currentUser) return;
    try {
      const response = await fetch('/api/notifications/delete', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ user_id: this.currentUser.id, notification_id: notifId })
      });
      const responseText = await response.text();
      let data = null;
      let invalidJson = false;
      try {
        data = responseText ? JSON.parse(responseText) : null;
      } catch {
        invalidJson = true;
      }
      if (!response.ok || !data || data.status !== 'ok') {
        const message = response.status === 404
          ? 'Máy chủ chưa được cập nhật API xóa thông báo. Hãy khởi động lại bằng run.bat.'
          : (invalidJson
            ? 'Máy chủ trả về dữ liệu không hợp lệ khi xóa thông báo.'
            : (data?.message || 'Không thể xóa thông báo.'));
        throw new Error(message);
      }
      document.getElementById(`notif-item-${notifId}`)?.remove();
      this.notificationItems = this.notificationItems.filter(entry => Number(entry.id) !== notifId);
      this.notificationOffset = Math.max(0, this.notificationOffset - 1);
      this.showToast('Đã xóa thông báo.', 'success');
      this.loadNotifications();
    } catch (error) {
      console.error('Lỗi xóa thông báo:', error);
      this.showToast(error.message || 'Không thể xóa thông báo.', 'error');
    }
  }

  toggleNotificationDropdown() {
    const dropdown = document.getElementById('notificationDropdown');
    if (!dropdown) return;
    if (dropdown.style.display === 'none' || !dropdown.style.display) {
      dropdown.style.display = 'flex';
      this.loadNotifications();
    } else {
      dropdown.style.display = 'none';
    }
  }

  async markAllNotificationsRead() {
    if (!this.currentUser) return;
    try {
      await fetch('/api/notifications/read', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ user_id: this.currentUser.id })
      });
      const badge = document.getElementById('unreadNotifCount');
      if (badge) badge.style.display = 'none';
      this.loadNotifications();
    } catch (e) {
      console.error('Lỗi đánh dấu tất cả thông báo đã đọc:', e);
      this.showToast('Không thể cập nhật thông báo.', 'error');
    }
  }

  async handleNotificationClick(link, notifId = 0) {
    if (notifId && this.currentUser) {
      try {
        await fetch('/api/notifications/read', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({
            user_id: this.currentUser.id,
            notification_id: notifId
          })
        });
        const notifEl = document.getElementById(`notif-item-${notifId}`);
        if (notifEl) {
          notifEl.classList.remove('unread');
          const dot = notifEl.querySelector('.unread-blue-dot');
          if (dot) dot.remove();
        }
        const badge = document.getElementById('unreadNotifCount');
        if (badge) {
          let cur = parseInt(badge.textContent || '0', 10);
          if (cur > 1) {
            badge.textContent = cur - 1;
          } else {
            badge.style.display = 'none';
          }
        }
      } catch (e) {
        console.error('Lỗi đánh dấu thông báo đã đọc:', e);
      }
    }

    if (!link) return;
    const linkedUrl = new URL(link, window.location.href);
    const postMatch = linkedUrl.hash.match(/^#post-(.+)$/);
    if (postMatch) {
      const postId = decodeURIComponent(postMatch[1]);
      await this.openCommentModal(postId);
      return;
    }
    window.location.href = link;
  }

  // =========================================================================
  // 9. GỢI Ý BẠN BÈ & BỘ LỌC GỢI Ý
  // =========================================================================
  async filterSuggestionsTab(filter, btn) {
    this.currentSuggestionFilter = filter;
    document.querySelectorAll('.suggestion-filter-pill').forEach(b => b.classList.remove('active'));
    if (btn) btn.classList.add('active');
    this.currentSuggestionLimit = 20;
    this.loadedSuggestionCount = 0;
    this.loadSuggestions(false);
  }

  async loadMoreSuggestionsFeed() {
    if (this.suggestionsLoading) return;
    const sidebarSuggestions = !!document.getElementById('suggestionsContainer');
    this.currentSuggestionLimit += sidebarSuggestions ? 5 : 20;
    await this.loadSuggestions(true);
  }

  async loadMoreSuggestions() {
    await this.loadMoreSuggestionsFeed();
  }

  async loadSuggestions(append = false) {
    const container = document.getElementById('suggestionsContainer') || document.getElementById('suggestionsGrid');
    if (!container || !this.currentUser) return;
    if (this.suggestionsLoading) {
      if (!append) this.suggestionsReloadQueued = true;
      return;
    }
    this.suggestionsLoading = true;
    const moreBtn = document.getElementById('btnLoadMoreSuggestionsFeed') || document.getElementById('btnLoadMoreSuggestions');

    try {
      if (!append) {
        this.loadedSuggestionCount = 0;
        this.stopObservingInfiniteScrollList(container);
      }
      const requestLimit = container.id === 'suggestionsContainer'
        ? 5 + Math.max(0, this.currentSuggestionLimit - 20)
        : this.currentSuggestionLimit;
      const res = await fetch(`/api/friends/suggest?user_id=${encodeURIComponent(this.currentUser.id)}&filter=${this.currentSuggestionFilter}&limit=${requestLimit}`);
      const data = await res.json();

      if (data.status === 'ok') {
        const suggestions = Array.isArray(data.suggestions) ? data.suggestions : [];
        if (suggestions.length === 0 && !append) {
          container.innerHTML = '<div style="font-size: 0.85rem; color: var(--text-muted); padding: 1rem; text-align: center;">Chưa tìm thấy gợi ý kết bạn phù hợp với bộ lọc này.</div>';
          if (moreBtn) moreBtn.style.display = 'none';
          this.stopObservingInfiniteScrollList(container);
          return;
        }

        if (moreBtn) {
          moreBtn.style.display = (suggestions.length >= requestLimit) ? 'inline-block' : 'none';
        }

        const pageItems = suggestions.slice(this.loadedSuggestionCount);
        const cards = pageItems.map(s => {
          const sugTarget = s.user_token || s.id;
          let buttonHtml = '';
          if (s.request_status === 'PENDING_SENT') {
            buttonHtml = `<button class="btn-secondary btn-pending-status" style="font-size: 0.8rem;" onclick="app.handleCancelFriendRequest(${app.jsArg(s.id)}, ${app.jsArg(s.name)})"><span class="btn-pending-label">⏳ Đã gửi lời mời</span><span class="btn-cancel-request-label">✕ Hủy gửi lời mời</span></button>`;
          } else if (s.request_status === 'PENDING_RECEIVED') {
            buttonHtml = `<button class="btn-primary" style="font-size: 0.8rem;" onclick="app.respondFriendRequest(${app.jsArg(s.id)}, 'ACCEPT')">Chấp nhận</button>`;
          } else if (s.request_status === 'FRIEND') {
            buttonHtml = `<span style="font-size: 0.8rem; color: var(--accent-green);">✓ Bạn bè</span>`;
          } else {
            buttonHtml = `<button class="btn-primary" style="font-size:.85rem;" onclick="app.sendFriendRequest(${app.jsArg(s.id)}, ${app.jsArg(s.name)})">Thêm bạn bè</button>`;
          }

          if (container.id === 'suggestionsGrid') {
            return `
              <div class="friend-card suggestion-friend-card" id="sug-${s.id}">
                <img class="fc-avatar" src="${this.escapeHtml(s.avatar || '/img/default-avatar.svg')}" onclick="app.openUserProfile(${app.jsArg(sugTarget)})" alt="">
                <div class="friend-card-content">
                  <h4 title="${this.escapeHtml(s.name)}" onclick="app.openUserProfile(${app.jsArg(sugTarget)})">${this.escapeHtml(s.name)}</h4>
                  <div class="fc-sub">${Number(s.mutual) > 0 ? `${Number(s.mutual)} bạn chung` : this.escapeHtml(s.reason || '')}</div>
                  <div class="friend-card-actions">
                    <div id="sug-btn-box-${s.id}">${buttonHtml}</div>
                    <button class="btn-secondary" type="button" onclick="document.getElementById('sug-${s.id}')?.remove()">Gỡ</button>
                  </div>
                </div>
              </div>`;
          }
          return `
            <div class="suggestion-item" id="sug-${s.id}">
              <div class="suggestion-user-info" onclick="app.openUserProfile(${app.jsArg(sugTarget)})" style="cursor: pointer;">
                <img src="${app.escapeHtml(s.avatar || '/img/default-avatar.svg')}" alt="Avatar">
                <div class="suggestion-user-details">
                  <h4>${this.escapeHtml(s.name)}</h4>
                  <div class="suggestion-reason">${this.escapeHtml(s.reason || '')}</div>
                  ${s.mutual > 0 ? `<div class="suggestion-mutual">${s.mutual} bạn chung</div>` : ''}
                </div>
              </div>
              <div id="sug-btn-box-${s.id}">${buttonHtml}</div>
            </div>
          `;
        }).join('');
        if (append) {
          const sentinel = container.querySelector(':scope > .infinite-scroll-sentinel');
          if (sentinel) sentinel.insertAdjacentHTML('beforebegin', cards);
          else container.insertAdjacentHTML('beforeend', cards);
        } else {
          container.innerHTML = cards;
        }
        this.loadedSuggestionCount = Math.min(requestLimit, suggestions.length);
        const hasMore = suggestions.length >= requestLimit && pageItems.length > 0;
        if (container.id === 'suggestionsGrid') {
          if (hasMore) this.observeInfiniteScrollList(container, () => this.loadMoreSuggestionsFeed());
          else this.stopObservingInfiniteScrollList(container);
        }
      }
    } catch (err) {
      console.error(err);
    } finally {
      this.suggestionsLoading = false;
      if (this.suggestionsReloadQueued) {
        this.suggestionsReloadQueued = false;
        this.loadSuggestions(false);
      }
    }
  }

  async sendFriendRequest(targetId, targetName = '') {
    if (!this.currentUser) return;
    try {
      const res = await fetch('/api/friends/request', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ my_id: this.currentUser.id, target_id: targetId })
      });
      const data = await res.json();
      if (data.status === 'ok') {
        this.showToast('Đã gửi lời mời kết bạn!', 'success');
        this.refreshSentRequestsSilently();
        // Lấy tên: ưu tiên tham số truyền vào, sau đó lấy từ suggestion item, cuối cùng là profFullname
        const resolvedName = targetName
          || document.querySelector(`#sug-${targetId} h4`)?.textContent
          || document.querySelector(`#feed-sug-${targetId} .feed-suggestion-name`)?.textContent
          || document.getElementById('profFullname')?.textContent
          || targetId;
        const pendingButton = `<button class="btn-secondary btn-pending-status" style="font-size:0.8rem;" onclick="app.handleCancelFriendRequest(${app.jsArg(targetId)}, ${app.jsArg(resolvedName)})"><span class="btn-pending-label">⏳ Đã gửi lời mời</span><span class="btn-cancel-request-label">✕ Hủy gửi lời mời</span></button>`;
        const suggestionBox = document.getElementById(`sug-btn-box-${targetId}`);
        if (suggestionBox) {
          suggestionBox.innerHTML = pendingButton;
        }
        const feedSuggestionBox = document.getElementById(`feed-sug-btn-box-${targetId}`);
        if (feedSuggestionBox) {
          feedSuggestionBox.innerHTML = pendingButton;
        }
        const profBtn = document.getElementById('profActionFriendBtn');
        if (profBtn) {
          profBtn.outerHTML = `
            <button class="btn-secondary btn-pending-status" id="profFriendStatusBtn" onclick="app.handleCancelFriendRequest(${app.jsArg(targetId)}, ${app.jsArg(resolvedName)})">
              <span class="btn-pending-label">⏳ Đã gửi lời mời</span>
              <span class="btn-cancel-request-label">✕ Hủy gửi lời mời</span>
            </button>
          `;
        }
        // Cập nhật nút trên thẻ tìm kiếm ngay lập tức
        const searchBtnBox = document.getElementById(`search-friend-btn-${targetId}`);
        if (searchBtnBox) {
          searchBtnBox.innerHTML = `<button class="btn-secondary btn-pending-status" type="button" onclick="app.handleCancelFriendRequest(${app.jsArg(targetId)}, ${app.jsArg(resolvedName)})"><span class="btn-pending-label">Đã gửi lời mời kết bạn</span><span class="btn-cancel-request-label">Hủy lời mời kết bạn</span></button>`;
        }
      } else {
        this.showToast(data.message || 'Lỗi gửi yêu cầu!', 'error');
      }
    } catch (err) {
      this.showToast('Lỗi: ' + err.message, 'error');
    }
  }

  async respondFriendRequest(targetId, action) {
    if (!this.currentUser) return;
    try {
      const res = await fetch('/api/friends/respond', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ my_id: this.currentUser.id, target_id: targetId, action: action })
      });
      const data = await res.json();
      if (data.status === 'ok') {
        this.showToast(data.message, 'success');
        // Cập nhật ngay nút trên thẻ gợi ý: Chấp nhận -> "✓ Bạn bè" (Xóa lời mời -> "Thêm bạn bè")
        const sugBox = document.getElementById(`sug-btn-box-${targetId}`);
        const feedSugBox = document.getElementById(`feed-sug-btn-box-${targetId}`);
        const keepAcceptedCard = (action === 'ACCEPT') && !!(sugBox || feedSugBox);
        if (action === 'ACCEPT') {
          const friendBtn = '<button class="btn-secondary" type="button" disabled style="font-size:0.85rem;color:var(--accent-green);cursor:default;">✓ Bạn bè</button>';
          if (sugBox) sugBox.innerHTML = friendBtn;
          if (feedSugBox) feedSugBox.innerHTML = friendBtn.replace('class="btn-secondary"', 'class="feed-suggestion-action btn-secondary"');
        } else {
          if (sugBox) sugBox.innerHTML = `<button class="btn-primary" type="button" style="font-size:0.85rem;" onclick="app.sendFriendRequest(${app.jsArg(targetId)})">Thêm bạn bè</button>`;
          if (feedSugBox) feedSugBox.innerHTML = `<button class="feed-suggestion-action btn-primary" onclick="app.sendFriendRequest(${app.jsArg(targetId)})">♟ Thêm bạn bè</button>`;
        }
        // Giữ thẻ vừa chấp nhận trên màn hình (hiện "✓ Bạn bè"), không tải lại danh sách gợi ý làm thẻ biến mất/dịch chỗ
        this.refreshSentRequestsSilently();
        const refreshTasks = [this.loadFriendsAndContacts()];
        if (!keepAcceptedCard) {
          refreshTasks.push(this.loadSuggestions());
          if (document.getElementById('feedFriendSuggestionsTrack')) {
            refreshTasks.push(this.loadFeedFriendSuggestions());
          }
        }
        if (window.location.pathname.includes('friends.html')) {
          refreshTasks.push(this.loadAllFriendsList(), this.loadPendingFriendRequests());
        }
        if (action === 'ACCEPT' && String(this.profileUserId) === String(targetId)) {
          refreshTasks.push(this.initProfilePage(this.profileUserId));
        }
        await Promise.all(refreshTasks);
      }
    } catch (err) {
      this.showToast('Lỗi: ' + err.message, 'error');
    }
  }

  // =========================================================================
  // 10. TRANG PROFILE RIÊNG BIỆT (profile.html?id=...)
  // =========================================================================
  openUserProfile(targetUserId) {
    if (!targetUserId) return;
    if (typeof targetUserId === 'object') {
      if (targetUserId.isGroup) return;
      targetUserId = targetUserId.user_token || targetUserId.id;
    }
    if (!targetUserId) return;
    window.location.href = `profile.html?id=${encodeURIComponent(targetUserId)}`;
  }

  openMyProfile() {
    if (!this.currentUser) return;
    // Dùng user_token (chuỗi ngẫu nhiên) trên URL thay vì ID nội bộ dạng ACC...
    const uid = this.currentUser.user_token || this.currentUser.id;
    if (window.location.pathname.includes('profile.html')) {
      window.history.pushState({}, '', `profile.html?id=${encodeURIComponent(uid)}`);
      this.initProfilePage(uid);
    } else {
      window.location.href = `profile.html?id=${encodeURIComponent(uid)}`;
    }
  }

  async initProfilePage(targetUserId) {
    let userId = targetUserId;
    if (!userId && this.currentUser) {
      userId = this.currentUser.user_token || this.currentUser.id;
    }
    if (!userId) {
      const stored = localStorage.getItem('cs_user');
      if (stored) {
        try {
          const parsed = JSON.parse(stored);
          userId = parsed.user_token || parsed.id;
        } catch(e) {}
      }
    }
    if (!userId) return;

    try {
      const res = await fetch(`/api/users/profile?id=${encodeURIComponent(userId)}&my_id=${this.currentUser ? this.currentUser.id : ''}`);
      const data = await res.json();

      if (data.status === 'ok') {
        const u = data.user;
        // Chuẩn hóa URL sang user_token (không lộ ID nội bộ ACC...)
        const publicId = u.user_token || u.id;
        if (publicId && window.location.pathname.includes('profile.html')) {
          const desired = `profile.html?id=${encodeURIComponent(publicId)}`;
          const current = window.location.pathname.split('/').pop() + (window.location.search || '');
          if (current !== desired && !current.endsWith(desired)) {
            window.history.replaceState({}, '', desired);
          }
        }
        document.title = `${u.fullname} - Trang cá nhân`;

        const profAv = document.getElementById('profAvatar');
        if (profAv) {
          profAv.src = u.avatar || '/img/default-avatar.svg';
          if (this.currentUser && this.currentUser.id === u.id) {
            profAv.onclick = () => this.openAvatarUploadModal();
            profAv.title = 'Nhấn để đổi ảnh đại diện';
            profAv.style.cursor = 'pointer';
          }
        }
        document.getElementById('profFullname').textContent = u.fullname;
        document.getElementById('profUsername').textContent = '@' + u.username;
        document.getElementById('profBio').textContent = u.bio || 'Chưa cập nhật tiểu sử';
        document.getElementById('profHometown').textContent = u.hometown || '--';
        if (document.getElementById('profResidence')) document.getElementById('profResidence').textContent = u.residence || '--';
        document.getElementById('profSchool').textContent = u.school || '--';
        document.getElementById('profWorkplace').textContent = u.workplace || '--';
        document.getElementById('profInterests').textContent = this.formatInterests(u.interests);
        if (document.getElementById('profGender')) document.getElementById('profGender').textContent = u.gender || '--';
        document.getElementById('profFriendCount').textContent = data.friend_count || 0;

        // Render nút hành động
        const actionRow = document.getElementById('profileActionsRow');
        if (actionRow) {
          if (this.currentUser && this.currentUser.id === u.id) {
            actionRow.innerHTML = `
              <button class="btn-primary" onclick="app.openEditProfileModal()">✏️ Chỉnh sửa hồ sơ</button>
              <button class="btn-secondary" onclick="app.openAvatarUploadModal()">🖼️ Đổi ảnh đại diện</button>
              <button class="btn-secondary" onclick="app.openBlockedUsersModal()">🚫 Người đã chặn</button>
            `;
          } else {
            let friendBtn = '';
            if (data.relation === 'BLOCKED' || data.relation === 'BLOCKED_BY_ME') {
              friendBtn = `<button class="btn-secondary" onclick="app.unblockUser(${app.jsArg(u.id)}, ${app.jsArg(u.fullname)})">✓ Đã chặn · Bỏ chặn</button>`;
            } else if (data.relation === 'FRIEND') {
              friendBtn = `
                <button class="btn-secondary btn-friend-status" id="profFriendStatusBtn" onclick="app.handleUnfriend(${app.jsArg(u.id)}, ${app.jsArg(u.fullname)})">
                  <span class="btn-friend-label">✓ Bạn bè</span>
                  <span class="btn-unfriend-label">✕ Xóa bạn</span>
                </button>
              `;
            } else if (data.relation === 'PENDING_SENT') {
              friendBtn = `
                <button class="btn-secondary btn-pending-status" id="profFriendStatusBtn" onclick="app.handleCancelFriendRequest(${app.jsArg(u.id)}, ${app.jsArg(u.fullname)})">
                  <span class="btn-pending-label">⏳ Đã gửi lời mời</span>
                  <span class="btn-cancel-request-label">✕ Hủy gửi lời mời</span>
                </button>
              `;
            } else if (data.relation === 'PENDING_RECEIVED') {
              friendBtn = `<button class="btn-primary" onclick="app.respondFriendRequest(${app.jsArg(u.id)}, 'ACCEPT')">✔️ Chấp nhận kết bạn</button>`;
            } else {
              friendBtn = `<button class="btn-primary" id="profActionFriendBtn" onclick="app.sendFriendRequest(${app.jsArg(u.id)})">+ Thêm bạn bè</button>`;
            }

            const isBlocked = (data.relation === 'BLOCKED' || data.relation === 'BLOCKED_BY_ME');
            actionRow.innerHTML = `
              ${friendBtn}
              ${isBlocked ? '' : `<button class="btn-secondary" onclick="app.openChatWith(${app.jsArg(u.id)}, ${app.jsArg(u.fullname)}, ${app.jsArg(u.avatar)})">💬 Nhắn tin</button>`}
              ${isBlocked ? '' : `<button class="btn-secondary" style="color:var(--accent-red);" onclick="app.blockUserFromProfile(${app.jsArg(u.id)}, ${app.jsArg(u.fullname)})">🚫 Chặn</button>`}
              <button class="btn-secondary" onclick="app.openReportModal(${app.jsArg(u.id)}, '')" title="Báo cáo người dùng">🚩</button>
            `;
          }
        }

        // Lưu profileUserId hiện tại và tải các bài viết (5 bài đầu tiên)
        this.profileUserId = u.id;
        this.loadUserPosts(u.id);

        // Tải danh sách bạn bè của người này (sử dụng u.id chuẩn)
        this.loadUserFriends(u.id);
      }
    } catch (err) {
      console.error(err);
    }
  }

  async loadUserPosts(userId, isLoadMore = false) {
    const list = document.getElementById('postsList');
    if (!list) return;
    this.profileUserId = userId;

    if (!isLoadMore) {
      this.profileOffset = 0;
      this.profileHasMore = true;
      this.isLoadingProfile = true;
      try {
        const res = await fetch(`/api/users/posts?author_id=${encodeURIComponent(userId)}&current_user_id=${this.currentUser ? this.currentUser.id : ''}&limit=5&offset=0`);
        const data = await res.json();

        if (data.status === 'ok') {
          if (!data.posts || data.posts.length === 0) {
            list.innerHTML = '<div style="text-align: center; padding: 2rem; color: var(--text-muted);">Người dùng này chưa có bài viết nào.</div>';
            this.profileHasMore = false;
            return;
          }
          this.profileOffset = data.posts.length;
          if (data.posts.length < 5) this.profileHasMore = false;
          list.innerHTML = data.posts.map(p => this.renderPostCard(p)).join('') +
            '<div id="profileScrollLoader" class="feed-scroll-loader" style="display:none;"><div class="spinner"></div> Đang tải thêm bài viết...</div>';
        }
      } catch (err) {
        list.innerHTML = '<div style="text-align: center; padding: 2rem; color: var(--accent-red);">Không thể tải bài viết!</div>';
      } finally {
        this.isLoadingProfile = false;
      }
    } else {
      if (!this.profileHasMore || this.isLoadingProfile) return;
      this.isLoadingProfile = true;
      const loader = document.getElementById('profileScrollLoader');
      if (loader) loader.style.display = 'flex';

      try {
        const res = await fetch(`/api/users/posts?author_id=${encodeURIComponent(userId)}&current_user_id=${this.currentUser ? this.currentUser.id : ''}&limit=5&offset=${this.profileOffset}`);
        const data = await res.json();
        if (data.status === 'ok' && Array.isArray(data.posts)) {
          if (data.posts.length === 0) {
            this.profileHasMore = false;
          } else {
            this.profileOffset += data.posts.length;
            if (data.posts.length < 5) this.profileHasMore = false;
            const newCardsHtml = data.posts.map(p => this.renderPostCard(p)).join('');
            if (loader) {
              loader.insertAdjacentHTML('beforebegin', newCardsHtml);
            } else {
              list.insertAdjacentHTML('beforeend', newCardsHtml);
            }
          }
        }
      } catch (err) {
        console.error('Lỗi tải thêm bài viết profile:', err);
      } finally {
        if (loader) loader.style.display = 'none';
        this.isLoadingProfile = false;
      }
    }
  }

  async loadUserFriends(userId) {
    try {
      const res = await fetch(`/api/friends/full?user_id=${encodeURIComponent(userId)}`);
      const data = await res.json();

      if (data.status === 'ok') {
        const previewGrid = document.getElementById('profFriendsPreviewGrid');
        const fullGrid = document.getElementById('profFullFriendsGrid');
        this.profileFriendsCache = Array.isArray(data.friends) ? data.friends : [];
        this.visibleProfileFriendsLimit = 20;

        if (previewGrid) {
          previewGrid.innerHTML = this.profileFriendsCache.slice(0, 6).map(f => `
            <div class="profile-friend-preview">
              <img src="${this.escapeHtml(f.avatar || '/img/default-avatar.svg')}" onclick="app.openUserProfile(${app.jsArg(f.user_token || f.id)})" alt="">
              <button type="button" class="profile-friend-preview-name" onclick="app.openUserProfile(${app.jsArg(f.user_token || f.id)})">${this.escapeHtml(f.fullname)}</button>
              <button type="button" class="profile-friend-chat" title="Nhắn tin" aria-label="Nhắn tin với ${this.escapeHtml(f.fullname)}"
                onclick="app.openChatWith(${app.jsArg(f.id)}, ${app.jsArg(f.fullname)}, ${app.jsArg(f.avatar || '')}, ${app.jsArg(f.user_token || f.id)})">💬</button>
            </div>
          `).join('');
        }

        if (fullGrid) {
          this.renderProfileFriendCards(false);
        }
      }
    } catch (err) {
      console.error(err);
    }
  }

  renderProfileFriendCards(append = false) {
    const grid = document.getElementById('profFullFriendsGrid');
    if (!grid) return;
    const friends = this.profileFriendsCache || [];
    if (!append) {
      grid.innerHTML = '';
      this.visibleProfileFriendsLimit = 20;
      this.stopObservingInfiniteScrollList(grid);
    }
    const start = append ? this.visibleProfileFriendsLimit - 20 : 0;
    const visible = friends.slice(start, this.visibleProfileFriendsLimit);
    const cards = visible.map(friend => `
      <div class="friend-card">
        <img class="fc-avatar" src="${this.escapeHtml(friend.avatar || '/img/default-avatar.svg')}" onclick="app.openUserProfile(${app.jsArg(friend.user_token || friend.id)})" alt="">
        <div class="friend-card-content">
          <h4 title="${this.escapeHtml(friend.fullname)}" onclick="app.openUserProfile(${app.jsArg(friend.user_token || friend.id)})">${this.escapeHtml(friend.fullname)}</h4>
          <div class="fc-sub">${this.escapeHtml(friend.school || friend.hometown || ('@' + friend.username))}</div>
          <div class="friend-card-actions">
            <button class="btn-primary" style="font-size:.8rem;" onclick="app.openChatWith(${app.jsArg(friend.id)}, ${app.jsArg(friend.fullname)}, ${app.jsArg(friend.avatar || '')}, ${app.jsArg(friend.user_token || friend.id)})">💬 Nhắn tin</button>
            <button class="btn-secondary" style="font-size:.8rem;" onclick="app.openUserProfile(${app.jsArg(friend.user_token || friend.id)})">Xem trang cá nhân</button>
          </div>
        </div>
      </div>`).join('');
    const sentinel = grid.querySelector(':scope > .infinite-scroll-sentinel');
    if (append && sentinel) sentinel.insertAdjacentHTML('beforebegin', cards);
    else if (append) grid.insertAdjacentHTML('beforeend', cards);
    else grid.innerHTML = cards;

    if (this.visibleProfileFriendsLimit < friends.length) {
      this.observeInfiniteScrollList(grid, () => this.loadMoreProfileFriends());
    } else {
      this.stopObservingInfiniteScrollList(grid);
    }
  }

  loadMoreProfileFriends() {
    if (this.visibleProfileFriendsLimit >= this.profileFriendsCache.length) return;
    this.visibleProfileFriendsLimit += 20;
    this.renderProfileFriendCards(true);
  }

  // =========================================================================
  // 11. TRANG BẠN BÈ RIÊNG BIỆT (friends.html)
  // =========================================================================
  initFriendsPage(initialTab = 'all') {
    this.switchFriendsPageTab(initialTab);
    this.loadAllFriendsList();
    this.loadPendingFriendRequests();
    this.loadSuggestions();
    this.refreshSidebarFriendBadge();
    if (initialTab === 'sent') this.loadSentFriendRequests();
    else this.refreshSentRequestsSilently();
  }

  switchFriendsPageTab(tab) {
    const normalized = (tab === 'search') ? 'all' : tab;
    ['tabAllFriendsBtn', 'tabRequestsBtn', 'tabSentBtn', 'tabSuggestionsBtn'].forEach(id => {
      document.getElementById(id)?.classList.remove('active');
    });
    ['tabContentAllFriends', 'tabContentRequests', 'tabContentSent', 'tabContentSuggestions'].forEach(id => {
      const el = document.getElementById(id);
      if (el) el.style.display = 'none';
    });

    // Highlight sidebar sub-item
    document.querySelectorAll('#sidebarNav [data-ftab]').forEach(a => {
      a.classList.toggle('active', a.getAttribute('data-ftab') === (tab === 'search' ? 'search' : normalized));
    });

    if (normalized === 'all') {
      document.getElementById('tabAllFriendsBtn')?.classList.add('active');
      const box = document.getElementById('tabContentAllFriends');
      if (box) box.style.display = 'block';
      if (tab === 'search') {
        setTimeout(() => document.getElementById('filterFriendsInput')?.focus(), 50);
      }
    } else if (normalized === 'requests') {
      document.getElementById('tabRequestsBtn')?.classList.add('active');
      const box = document.getElementById('tabContentRequests');
      if (box) box.style.display = 'block';
    } else if (normalized === 'sent') {
      document.getElementById('tabSentBtn')?.classList.add('active');
      const box = document.getElementById('tabContentSent');
      if (box) box.style.display = 'block';
      this.loadSentFriendRequests();
    } else if (normalized === 'suggestions') {
      document.getElementById('tabSuggestionsBtn')?.classList.add('active');
      const box = document.getElementById('tabContentSuggestions');
      if (box) box.style.display = 'block';
    }
  }

  async loadSentFriendRequests(silent = false) {
    if (!this.currentUser) return;
    const grid = document.getElementById('sentRequestsGrid');
    if (!grid) return;
    if (!silent) grid.innerHTML = '<div style="text-align:center;padding:2rem;color:var(--text-muted);grid-column:1/-1;">Đang tải...</div>';
    try {
      const res = await fetch(`/api/friends/requests/sent?user_id=${encodeURIComponent(this.currentUser.id)}&_=${Date.now()}`, { cache: 'no-store' });
      const data = await res.json();
      const list = (data.status === 'ok' && Array.isArray(data.requests)) ? data.requests : [];
      const sentBadge = document.getElementById('sentCountBadge');
      if (sentBadge) sentBadge.textContent = list.length;
      if (!list.length) {
        grid.innerHTML = '<div style="text-align:center;padding:2rem;color:var(--text-muted);grid-column:1/-1;">Bạn chưa gửi lời mời kết bạn nào.</div>';
        return;
      }
      grid.innerHTML = list.map(u => `
        <div class="friend-card" style="background:var(--bg-surface);border:1px solid var(--border-color);border-radius:12px;padding:1rem;display:flex;flex-direction:column;gap:.6rem;">
          <div style="display:flex;gap:.75rem;align-items:center;cursor:pointer;" onclick="app.openUserProfile({id:${app.jsArg(u.id)},name:${app.jsArg(u.fullname || '')},avatar:${app.jsArg(u.avatar || '')}})">
            <img src="${this.escapeHtml(u.avatar || '/img/default-avatar.svg')}" style="width:56px;height:56px;border-radius:50%;object-fit:cover;" onerror="this.src='/img/default-avatar.svg'">
            <div style="min-width:0;">
              <div style="font-weight:700;">${this.escapeHtml(u.fullname || 'Người dùng')}</div>
              <div style="font-size:.8rem;color:var(--text-muted);">Đã gửi lời mời · ${this.formatTimeAgo(u.created_at || '')}</div>
            </div>
          </div>
          <button class="btn-secondary" style="width:100%;" onclick="app.cancelFriendRequest(${app.jsArg(u.id)})">Hủy lời mời</button>
        </div>`).join('');
    } catch (e) {
      console.error(e);
      grid.innerHTML = '<div style="text-align:center;padding:2rem;color:var(--text-muted);grid-column:1/-1;">Không thể tải danh sách.</div>';
    }
  }

  // Làm mới âm thầm danh sách + số đếm "Đã gửi" (gọi sau khi gửi/hủy/chấp nhận lời mời)
  refreshSentRequestsSilently() {
    if (document.getElementById('sentRequestsGrid') || document.getElementById('sentCountBadge')) {
      this.loadSentFriendRequests(true);
    }
  }

  async cancelFriendRequest(targetId) {
    if (!this.currentUser || !targetId) return;
    try {
      const res = await fetch('/api/friends/cancel', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ my_id: this.currentUser.id, target_id: targetId })
      });
      const data = await res.json();
      if (data.status === 'ok') {
        this.showToast('Đã hủy lời mời kết bạn.', 'success');
        this.loadSentFriendRequests();
      } else {
        this.showToast(data.message || 'Không thể hủy lời mời.', 'error');
      }
    } catch (e) {
      this.showToast('Lỗi: ' + e.message, 'error');
    }
  }

  async loadAllFriendsList() {
    if (!this.currentUser) return;
    const grid = document.getElementById('allFriendsGrid');
    if (!grid) return;

    try {
      const res = await fetch(`/api/friends/full?user_id=${this.currentUser.id}`);
      const data = await res.json();

      if (data.status === 'ok') {
        this.fullFriendsCache = data.friends || [];
        this.filteredFriendsCache = this.fullFriendsCache;
        const countBadge = document.getElementById('allFriendsCount');
        if (countBadge) countBadge.textContent = this.fullFriendsCache.length;

        this.visibleFriendsLimit = 20;
        this.renderFriendsCards(this.fullFriendsCache);
      }
    } catch (e) {
      console.error(e);
    }
  }

  renderFriendsCards(friends) {
    const grid = document.getElementById('allFriendsGrid');
    if (!grid) return;
    this.filteredFriendsCache = friends || [];
    this.visibleFriendsLimit = 20;
    this.stopObservingInfiniteScrollList(grid);

    if (!friends || friends.length === 0) {
      grid.innerHTML = '<div style="text-align: center; padding: 2rem; color: var(--text-muted); grid-column: 1 / -1;">Bạn chưa kết bạn với ai. Hãy khám phá phần Gợi ý bạn bè!</div>';
      return;
    }

    const cards = friends.slice(0, this.visibleFriendsLimit).map(f => `
      <div class="friend-card" id="friend-card-${f.id}">
        <!-- Nhấn vào avatar để sang web profile của bạn bè (dùng user_token) -->
        <img class="fc-avatar" src="${app.escapeHtml(f.avatar || '/img/default-avatar.svg')}" onclick="app.openUserProfile(${app.jsArg(f.user_token || f.id)})" title="Nhấn để xem trang cá nhân">
        <div class="friend-card-content">
          <h4 onclick="app.openUserProfile(${app.jsArg(f.user_token || f.id)})">${this.escapeHtml(f.fullname)}</h4>
          <div class="fc-sub">${this.escapeHtml(f.school || f.hometown || ('@' + f.username))}</div>
          <div class="friend-card-actions">
            <button class="btn-primary" style="font-size: 0.8rem;" onclick="app.openChatWith(${app.jsArg(f.id)}, ${app.jsArg(f.fullname)}, ${app.jsArg(f.avatar)})">💬 Nhắn tin</button>
            <button class="btn-secondary" style="font-size: 0.8rem;" onclick="app.openUserProfile(${app.jsArg(f.user_token || f.id)})">Xem trang cá nhân</button>
          </div>
        </div>
      </div>
    `).join('');
    grid.innerHTML = cards;
    if (friends.length > this.visibleFriendsLimit) {
      this.observeInfiniteScrollList(grid, () => this.loadMoreFriends());
    }
  }

  loadMoreFriends() {
    const grid = document.getElementById('allFriendsGrid');
    const friends = this.filteredFriendsCache || [];
    if (!grid || this.visibleFriendsLimit >= friends.length) return;
    const nextLimit = Math.min(this.visibleFriendsLimit + 20, friends.length);
    const cards = friends.slice(this.visibleFriendsLimit, nextLimit).map(f => `
      <div class="friend-card" id="friend-card-${this.escapeHtml(f.id)}">
        <img class="fc-avatar" src="${this.escapeHtml(f.avatar || '/img/default-avatar.svg')}" onclick="app.openUserProfile(${app.jsArg(f.user_token || f.id)})" title="Nhấn để xem trang cá nhân" alt="">
        <div class="friend-card-content">
          <h4 onclick="app.openUserProfile(${app.jsArg(f.user_token || f.id)})">${this.escapeHtml(f.fullname)}</h4>
          <div class="fc-sub">${this.escapeHtml(f.school || f.hometown || ('@' + f.username))}</div>
          <div class="friend-card-actions">
            <button class="btn-primary" style="font-size:.8rem;" onclick="app.openChatWith(${app.jsArg(f.id)}, ${app.jsArg(f.fullname)}, ${app.jsArg(f.avatar || '')}, ${app.jsArg(f.user_token || f.id)})">💬 Nhắn tin</button>
            <button class="btn-secondary" style="font-size:.8rem;" onclick="app.openUserProfile(${app.jsArg(f.user_token || f.id)})">Xem trang cá nhân</button>
          </div>
        </div>
      </div>`).join('');
    const sentinel = grid.querySelector(':scope > .infinite-scroll-sentinel');
    if (sentinel) sentinel.insertAdjacentHTML('beforebegin', cards);
    else grid.insertAdjacentHTML('beforeend', cards);
    this.visibleFriendsLimit = nextLimit;
    if (nextLimit >= friends.length) this.stopObservingInfiniteScrollList(grid);
  }

  filterFriendsList(keyword) {
    if (!this.fullFriendsCache) return;
    const q = keyword.toLowerCase().trim();
    if (!q) {
      this.renderFriendsCards(this.fullFriendsCache);
      return;
    }
    const filtered = this.fullFriendsCache.filter(f =>
      f.fullname.toLowerCase().includes(q) ||
      f.username.toLowerCase().includes(q) ||
      (f.school && f.school.toLowerCase().includes(q))
    );
    this.renderFriendsCards(filtered);
  }

  async loadPendingFriendRequests() {
    if (!this.currentUser) return;
    const grid = document.getElementById('requestsGrid');
    if (!grid) return;

    try {
      const res = await fetch(`/api/friends/requests?user_id=${this.currentUser.id}`);
      const data = await res.json();

      if (data.status === 'ok') {
        this.pendingRequestsCache = Array.isArray(data.requests) ? data.requests : [];
        this.visibleRequestsLimit = 20;
        const badge = document.getElementById('reqCountBadge');
        if (badge) badge.textContent = this.pendingRequestsCache.length;

        if (this.pendingRequestsCache.length === 0) {
          grid.innerHTML = '<div style="text-align: center; padding: 2rem; color: var(--text-muted); grid-column: 1 / -1;">Không có lời mời kết bạn nào đang chờ.</div>';
          this.stopObservingInfiniteScrollList(grid);
          return;
        }

        const cards = this.pendingRequestsCache.slice(0, this.visibleRequestsLimit).map(u => `
          <div class="friend-card">
            <img class="fc-avatar" src="${app.escapeHtml(u.avatar || '/img/default-avatar.svg')}" onclick="app.openUserProfile(${app.jsArg(u.user_token || u.id)})">
            <div class="friend-card-content">
              <h4 onclick="app.openUserProfile(${app.jsArg(u.user_token || u.id)})">${this.escapeHtml(u.fullname)}</h4>
              <div class="fc-sub">${this.escapeHtml(u.school || u.hometown || '')}</div>
              <div class="friend-card-actions">
                <button class="btn-primary" style="font-size: 0.8rem;" onclick="app.respondFriendRequest(${app.jsArg(u.id)}, 'ACCEPT')">Chấp nhận</button>
                <button class="btn-secondary" style="font-size: 0.8rem;" onclick="app.respondFriendRequest(${app.jsArg(u.id)}, 'REJECT')">Xóa lời mời</button>
              </div>
            </div>
          </div>
        `).join('');
        grid.innerHTML = cards;
        if (this.pendingRequestsCache.length > this.visibleRequestsLimit) {
          this.observeInfiniteScrollList(grid, () => this.loadMoreFriendRequests());
        } else {
          this.stopObservingInfiniteScrollList(grid);
        }
      }
    } catch (e) {
      console.error(e);
    }
  }

  loadMoreFriendRequests() {
    const grid = document.getElementById('requestsGrid');
    if (!grid || this.visibleRequestsLimit >= this.pendingRequestsCache.length) return;
    const nextLimit = Math.min(this.visibleRequestsLimit + 20, this.pendingRequestsCache.length);
    const cards = this.pendingRequestsCache.slice(this.visibleRequestsLimit, nextLimit).map(user => `
      <div class="friend-card">
        <img class="fc-avatar" src="${this.escapeHtml(user.avatar || '/img/default-avatar.svg')}" onclick="app.openUserProfile(${app.jsArg(user.user_token || user.id)})" alt="">
        <div class="friend-card-content">
          <h4 onclick="app.openUserProfile(${app.jsArg(user.user_token || user.id)})">${this.escapeHtml(user.fullname)}</h4>
          <div class="fc-sub">${this.escapeHtml(user.school || user.hometown || '')}</div>
          <div class="friend-card-actions">
            <button class="btn-primary" style="font-size:.8rem;" onclick="app.respondFriendRequest(${app.jsArg(user.id)}, 'ACCEPT')">Chấp nhận</button>
            <button class="btn-secondary" style="font-size:.8rem;" onclick="app.respondFriendRequest(${app.jsArg(user.id)}, 'REJECT')">Xóa lời mời</button>
          </div>
        </div>
      </div>`).join('');
    const sentinel = grid.querySelector(':scope > .infinite-scroll-sentinel');
    if (sentinel) sentinel.insertAdjacentHTML('beforebegin', cards);
    else grid.insertAdjacentHTML('beforeend', cards);
    this.visibleRequestsLimit = nextLimit;
    if (nextLimit >= this.pendingRequestsCache.length) this.stopObservingInfiniteScrollList(grid);
  }

  // =========================================================================
  // 11b. ĐĂNG LẠI BÀI VIẾT (REPOST) LÊN TRANG CÁ NHÂN
  // =========================================================================
  async openRepostModal(postId) {
    if (!this.currentUser) {
      this.showToast('Vui lòng đăng nhập để đăng lại bài viết!', 'error');
      return;
    }
    this.repostTargetPostId = postId;
    const modal = document.getElementById('repostModal');
    if (!modal) return;

    const preview = document.getElementById('repostPreviewBox');
    const thoughts = document.getElementById('repostThoughtsInput');
    const vis = document.getElementById('repostVisibilitySelect');
    if (thoughts) thoughts.value = '';
    if (vis) vis.value = 'PUBLIC';

    if (preview) {
      preview.innerHTML = '<div style="color:var(--text-muted); text-align:center;">Đang tải bài viết...</div>';
      try {
        // Tìm trong feed cache trước
        let post = null;
        if (this.feedPostsCache && Array.isArray(this.feedPostsCache)) {
          post = this.feedPostsCache.find(p => p.id === postId);
        }
        if (!post && this.userPostsCache && Array.isArray(this.userPostsCache)) {
          post = this.userPostsCache.find(p => p.id === postId);
        }
        if (!post) {
          const res = await fetch(`/api/posts/detail?id=${encodeURIComponent(postId)}&user_id=${encodeURIComponent(this.currentUser.id)}`);
          const data = await res.json();
          if (data.status === 'ok' && data.post) post = data.post;
        }
        if (post) {
          // Nếu đang repost một bài đã là repost → hiển thị gốc
          const display = (post.shared_post_id && post.original_post) ? post.original_post : post;
          const author = display.author_name || post.author_name || 'Người dùng';
          const content = display.content || post.content || '';
          const title = display.title || post.title || '';
          preview.innerHTML = `
            <div style="display:flex; gap:0.5rem; align-items:center; margin-bottom:0.4rem;">
              <img src="${app.escapeHtml(display.author_avatar || post.author_avatar || '/img/default-avatar.svg')}" style="width:32px;height:32px;border-radius:50%;object-fit:cover;" alt="">
              <div>
                <div style="font-weight:700;">${this.escapeHtml(author)}</div>
                <div style="font-size:0.75rem;color:var(--text-muted);">${this.formatTimeAgo(display.created_at || post.created_at)}</div>
              </div>
            </div>
            ${title ? `<div style="font-weight:600;margin-bottom:0.25rem;">${this.escapeHtml(title)}</div>` : ''}
            <div style="white-space:pre-wrap;word-break:break-word;">${this.escapeHtml(content)}</div>
          `;
        } else {
          preview.innerHTML = '<div style="color:var(--text-muted);">Không tải được nội dung bài viết gốc.</div>';
        }
      } catch (e) {
        preview.innerHTML = '<div style="color:var(--accent-red);">Lỗi tải bài viết.</div>';
      }
    }

    modal.style.display = 'flex';
  }

  closeRepostModal() {
    const modal = document.getElementById('repostModal');
    if (modal) modal.style.display = 'none';
    this.repostTargetPostId = null;
  }

  async submitRepost() {
    if (!this.currentUser || !this.repostTargetPostId) return;
    const thoughts = (document.getElementById('repostThoughtsInput')?.value || '').trim();
    const visibility = document.getElementById('repostVisibilitySelect')?.value || 'PUBLIC';

    try {
      const res = await fetch('/api/posts/repost', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          user_id: this.currentUser.id,
          post_id: this.repostTargetPostId,
          thoughts: thoughts,
          visibility: visibility
        })
      });
      const data = await res.json();
      if (data.status === 'ok') {
        this.showToast(data.message || 'Đã đăng lại bài viết lên trang cá nhân!', 'success');
        this.closeRepostModal();
        // Làm mới feed nếu đang ở trang chủ
        if (typeof this.loadFeed === 'function') {
          this.loadFeed(false);
        }
      } else {
        this.showToast(data.message || 'Không thể đăng lại bài viết!', 'error');
      }
    } catch (err) {
      this.showToast('Lỗi: ' + err.message, 'error');
    }
  }

  // 12. CHIA SẺ BÀI VIẾT VÀO HỘP THOẠI CHAT BẠN BÈ / NHÓM
  // =========================================================================
  async openShareToChatModal(postId) {
    this.shareTargetPostId = postId;
    const modal = document.getElementById('shareToChatModal');
    if (!modal) return;

    const select = document.getElementById('shareChatTargetSelect');
    if (select) {
      select.innerHTML = '<option value="">Đang tải danh bạ...</option>';
      try {
        const resFriends = await fetch(`/api/friends/contacts?user_id=${this.currentUser.id}`);
        const dataFriends = await resFriends.json();

        const resGroups = await fetch(`/api/groups/my-groups?user_id=${this.currentUser.id}`);
        const dataGroups = await resGroups.json();

        let optionsHtml = '';
        if (dataFriends.status === 'ok' && dataFriends.contacts.length > 0) {
          optionsHtml += `<optgroup label="Bạn bè">`;
          dataFriends.contacts.forEach(f => {
            optionsHtml += `<option value="USER:${f.id}">👤 ${this.escapeHtml(f.name)}</option>`;
          });
          optionsHtml += `</optgroup>`;
        }

        if (dataGroups.status === 'ok' && dataGroups.groups.length > 0) {
          optionsHtml += `<optgroup label="Nhóm chat">`;
          dataGroups.groups.forEach(g => {
            optionsHtml += `<option value="GROUP:${g.id}">👥 ${this.escapeHtml(g.name)}</option>`;
          });
          optionsHtml += `</optgroup>`;
        }

        select.innerHTML = optionsHtml || '<option value="">Chưa có bạn bè hoặc nhóm nào</option>';
      } catch (e) {
        select.innerHTML = '<option value="">Lỗi tải danh bạ</option>';
      }
    }

    const msgInput = document.getElementById('shareChatMessageInput');
    if (msgInput) msgInput.value = '';

    modal.style.display = 'flex';
  }

  closeShareToChatModal() {
    const modal = document.getElementById('shareToChatModal');
    if (modal) modal.style.display = 'none';
    this.shareTargetPostId = null;
  }

  async submitSharePostToChat() {
    if (!this.shareTargetPostId || !this.currentUser) return;
    const select = document.getElementById('shareChatTargetSelect');
    const val = select ? select.value : '';
    if (!val) {
      this.showToast('Vui lòng chọn người nhận hoặc nhóm!', 'error');
      return;
    }

    const [type, targetId] = val.split(':');
    const comment = document.getElementById('shareChatMessageInput')?.value.trim() || '';

    try {
      const res = await fetch('/api/messages/share-post', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          sender_id: this.currentUser.id,
          target_type: type,
          target_id: targetId,
          post_id: this.shareTargetPostId,
          comment: comment
        })
      });
      const data = await res.json();
      if (data.status === 'ok') {
        this.showToast(data.message, 'success');
        this.closeShareToChatModal();
        await this.loadFriendsAndContacts();
      } else {
        this.showToast(data.message || 'Lỗi chia sẻ bài viết!', 'error');
      }
    } catch (err) {
      this.showToast('Lỗi: ' + err.message, 'error');
    }
  }

  // =========================================================================
  // 13. HỘP THOẠI TRÒ CHUYỆN (1-ON-1 VÀ NHẮN TIN NHÓM)
  // =========================================================================
  async loadFriendsAndContacts() {
    const container = document.getElementById('contactsContainer');
    if (!container || !this.currentUser) return;

    try {
      const [friendsRes, groupsRes] = await Promise.all([
        fetch(`/api/friends/contacts?user_id=${this.currentUser.id}`),
        fetch(`/api/groups/my-groups?user_id=${this.currentUser.id}`)
      ]);

      const friendsData = await friendsRes.json();
      const groupsData = await groupsRes.json();

      let friends = (friendsData.status === 'ok' && friendsData.contacts) ? friendsData.contacts.map(c => ({
        ...c,
        itemType: 'friend'
      })) : [];

      let groups = (groupsData.status === 'ok' && groupsData.groups) ? groupsData.groups.map(g => ({
        id: g.id,
        name: g.name,
        avatar: g.avatar || '/img/default-avatar.svg',
        last_message: g.last_message || '',
        last_message_time: g.last_message_time || g.created_at || '',
        last_message_sender: g.last_message_sender || '',
        last_message_sender_id: g.last_message_sender_id || '',
        last_message_media_type: g.last_message_media_type || 'NONE',
        is_unread: !!g.is_unread,
        is_archived: !!g.is_archived,
        itemType: 'group'
      })) : [];

      this.allContactsCache = {
        friends: friends,
        groups: groups
      };

      this.renderContactsList(this.currentContactFilter || 'all');
    } catch (err) {
      console.error('Lỗi load danh bạ:', err);
    }
  }

  filterContacts(filter, btn) {
    this.currentContactFilter = filter;
    const title = document.querySelector('.chat-list-header-actions')?.parentElement?.querySelector('span');
    if (title) {
      title.textContent = filter === 'archived' ? 'Đoạn chat lưu trữ'
        : filter === 'pending' ? 'Tin nhắn đang chờ'
          : filter === 'blocked' ? 'Tài khoản đã bị chặn'
            : 'Đoạn Chat';
    }
    const pills = document.querySelectorAll('.chat-filter-pill');
    pills.forEach(p => p.classList.remove('active'));
    if (btn) btn.classList.add('active');
    this.renderContactsList(filter);
  }

  renderContactsList(filter = 'all') {
    const container = document.getElementById('contactsContainer');
    if (!container || !this.allContactsCache) return;

    let items = [];
    if (filter === 'pending') {
      items = this.pendingMessageContacts;
    } else if (filter === 'all' || filter === 'archived') {
      items = [...this.allContactsCache.friends, ...this.allContactsCache.groups];
    } else if (filter === 'friends') {
      items = this.allContactsCache.friends;
    } else if (filter === 'unread') {
      items = [...this.allContactsCache.friends, ...this.allContactsCache.groups].filter(c => c.is_unread);
    } else if (filter === 'groups') {
      items = this.allContactsCache.groups;
    }
    if (filter !== 'pending') {
      items = items.filter(c => filter === 'archived' ? c.is_archived : !c.is_archived);
    }

    // Sắp xếp người hoặc nhóm nhắn tin gần nhất lên đầu (Item 11)
    items.sort((a, b) => {
      const timeA = a.last_message_time || '';
      const timeB = b.last_message_time || '';
      return timeB.localeCompare(timeA);
    });

    if (items.length === 0) {
      let emptyMsg = 'Không có cuộc trò chuyện nào.';
      if (filter === 'unread') emptyMsg = 'Không có tin nhắn chưa đọc.';
      if (filter === 'groups') emptyMsg = 'Chưa tham gia nhóm chat nào.';
      if (filter === 'friends') emptyMsg = 'Chưa có bạn bè nào.';
      if (filter === 'archived') emptyMsg = 'Không có đoạn chat nào được lưu trữ.';
      if (filter === 'pending') emptyMsg = 'Không có tin nhắn đang chờ.';
      container.innerHTML = `<div style="font-size: 0.8rem; color: var(--text-muted); padding: 0.75rem; text-align: center;">${emptyMsg}</div>`;
      return;
    }

    // Cắt ngắn tin nhắn dài bằng dấu ... (Item 12)
    const truncate = (str, len = 24) => {
      if (!str) return '';
      return str.length > len ? str.substring(0, len) + '...' : str;
    };

    container.innerHTML = items.map(c => {
      if (c.itemType === 'pending') {
        const preview = c.last_message === 'Tin nhắn đã được thu hồi' ? c.last_message
          : c.last_message_media_type === 'IMAGE' ? 'Đã gửi Hình ảnh'
            : c.last_message_media_type === 'VIDEO' ? 'Đã gửi Video'
            : c.last_message || 'Tin nhắn mới';
        const isUnreadPending = !!c.is_unread;
        return `
          <div class="contact-item pending-message-contact ${isUnreadPending ? 'has-unread' : ''}"
               data-contact-id="${this.escapeHtml(c.id)}" data-contact-type="pending"
               data-contact-name="${this.escapeHtml(c.name)}" data-contact-avatar="${this.escapeHtml(c.avatar || '')}"
               onclick="app.openChatWith(${app.jsArg(c.id)}, ${app.jsArg(c.name)}, ${app.jsArg(c.avatar || '')}, ${app.jsArg(c.user_token || c.id)})">
            <div class="contact-avatar-box">
              <img src="${this.escapeHtml(c.avatar || '/img/default-avatar.svg')}" alt="Avatar">
            </div>
            <div class="contact-info">
              <div class="contact-name ${isUnreadPending ? 'unread-name' : ''}">${this.escapeHtml(c.name)}</div>
              <div class="contact-last-msg ${isUnreadPending ? 'unread-msg' : 'read-msg'}">${this.escapeHtml(truncate(preview))}</div>
            </div>
            ${isUnreadPending ? '<span class="unread-blue-dot" title="Tin nhắn đang chờ"></span>' : ''}
          </div>
        `;
      }
      if (c.itemType === 'group') {
        let groupLastMsg = c.last_message || 'Nhóm chat';
        const groupMediaType = String(c.last_message_media_type || 'NONE').toUpperCase();
          if (groupMediaType === 'IMAGE' && groupLastMsg !== 'Tin nhắn đã được thu hồi') groupLastMsg = 'đã gửi Hình ảnh';
          else if (groupMediaType === 'VIDEO' && groupLastMsg !== 'Tin nhắn đã được thu hồi') groupLastMsg = 'đã gửi Video';
          else if (!groupLastMsg.trim()) groupLastMsg = 'Nhóm chat';
        if (typeof groupLastMsg === 'string') {
          if (groupLastMsg.startsWith('__SYS_RENAME__:')) {
            const p = groupLastMsg.split(':');
            const newName = p.slice(2).join(':') || '';
            groupLastMsg = newName ? `Đã đổi tên nhóm thành "${newName}"` : 'Đã đổi tên nhóm';
          } else if (groupLastMsg.startsWith('__SYS_GROUP_INVITE__:')) {
            try {
              const names = JSON.parse(groupLastMsg.slice('__SYS_GROUP_INVITE__:'.length));
              groupLastMsg = `${c.last_message_sender || 'Thành viên'} đã mời ${names.join(', ')} vào nhóm`;
            } catch (error) {
              groupLastMsg = 'Thông báo nhóm';
            }
          }
        }
        if (groupMediaType !== 'SYSTEM' && !groupLastMsg.startsWith('__SYS_')) {
          const sender = c.last_message_sender_id === this.currentUser?.id ? 'Bạn' : c.last_message_sender;
          if (sender) groupLastMsg = `${sender}: ${groupLastMsg}`;
        }
        // Áp dụng cùng quy tắc hiển thị như chat 1-1: chưa đọc = đậm, đã đọc/tin của mình = nhạt
        const isUnreadG = !!c.is_unread;
        const msgClassG = isUnreadG ? 'contact-last-msg unread-msg' : 'contact-last-msg read-msg';
        const nameClassG = isUnreadG ? 'contact-name unread-name' : 'contact-name';
        const blueDotHtmlG = isUnreadG ? '<span class="unread-blue-dot" title="Tin nhắn chưa đọc"></span>' : '';
        return `
          <div class="contact-item ${isUnreadG ? 'has-unread' : ''}" data-contact-id="${this.escapeHtml(c.id)}" data-contact-type="group" data-contact-name="${this.escapeHtml(c.name)}" data-contact-avatar="${this.escapeHtml(c.avatar || '')}" onclick="app.openGroupChat(${app.jsArg(c.id)}, ${app.jsArg(c.name)}, ${app.jsArg(c.avatar || '')})">
            <div class="contact-avatar-box">
              <img src="${app.escapeHtml(c.avatar || '/img/default-avatar.svg')}" alt="Group" class="group-avatar-clickable"
                   onclick="app.openImageLightbox('${(c.avatar || '/img/default-avatar.svg').replace(/'/g, "\\'")}', event)" title="Xem ảnh nhóm">
            </div>
            <div class="contact-info">
              <div class="${nameClassG}">
                <span>👥 ${this.escapeHtml(c.name)}</span>
                ${blueDotHtmlG}
              </div>
              <div class="${msgClassG}">${this.escapeHtml(truncate(groupLastMsg))}</div>
            </div>
            ${this.renderContactOptions(c)}
          </div>
        `;
      } else {
        const isMine = c.last_message_is_mine;
        const isUnread = c.is_unread;
        let prefix = '';
        let statusNote = '';
        if (isMine) {
          statusNote = c.is_seen_by_partner ? ' · Đã xem' : ' · Đã gửi';
        }

        // Quyết định class hiển thị:
        // - Nếu là tin nhắn chưa đọc (đối phương gửi đến mà mình chưa đọc): hiện màu đen đậm (unread-msg, unread-name) và dấu chấm màu xanh dương!
        // - Nếu đã xem (hoặc tin nhắn cũ đã đọc): hiện chữ nhạt hơn (read-msg)!
        const displayName = c.nickname || c.name;
        let lastMsgText = c.last_message || 'Chưa có tin nhắn';
        const lastMediaType = String(c.last_message_media_type || 'NONE').toUpperCase();
        if (!c.last_message && lastMediaType === 'IMAGE') lastMsgText = 'đã gửi Hình ảnh';
        else if (!c.last_message && lastMediaType === 'VIDEO') lastMsgText = 'đã gửi Video';
        if (typeof lastMsgText === 'string') {
          if (lastMsgText.startsWith('__SYS_NICK__:')) {
            const p = lastMsgText.split(':');
            lastMsgText = 'Đã đặt biệt danh: ' + (p.slice(3).join(':') || 'mới');
          } else if (lastMsgText.startsWith('__SYS_NICK_REMOVE__:')) {
            lastMsgText = 'Đã gỡ biệt danh';
          } else if (lastMsgText.startsWith('__SYS_RENAME__:')) {
            const p = lastMsgText.split(':');
            const newName = p.slice(2).join(':') || '';
            lastMsgText = newName ? `Đã đổi tên nhóm thành "${newName}"` : 'Đã đổi tên nhóm';
          }
        }

        const msgClass = isUnread ? 'contact-last-msg unread-msg' : 'contact-last-msg read-msg';
        const nameClass = isUnread ? 'contact-name unread-name' : 'contact-name';
        const blueDotHtml = isUnread ? '<span class="unread-blue-dot" title="Tin nhắn chưa đọc"></span>' : '';
        const lastSenderLabel = isMine ? 'Bạn' : (c.real_name || c.name);
        const messagePreview = lastMediaType === 'IMAGE' && !c.last_message
          ? 'đã gửi Hình ảnh'
          : lastMediaType === 'VIDEO' && !c.last_message ? 'đã gửi Video' : lastMsgText;

        return `
          <div class="contact-item ${isUnread ? 'has-unread' : ''}" data-contact-id="${this.escapeHtml(c.id)}" data-contact-type="friend" data-contact-name="${this.escapeHtml(displayName)}" data-contact-avatar="${this.escapeHtml(c.avatar || '')}" data-contact-token="${this.escapeHtml(c.user_token || c.id)}" onclick="app.openChatWith(${app.jsArg(c.id)}, ${app.jsArg(displayName)}, ${app.jsArg(c.avatar)}, ${app.jsArg(c.user_token || c.id)})">
            <div class="contact-avatar-box">
              <img src="${app.escapeHtml(c.avatar || '/img/default-avatar.svg')}" alt="Avatar">
              <span class="online-indicator ${c.is_online ? 'online' : 'offline'}"></span>
            </div>
            <div class="contact-info">
              <div class="${nameClass}">
                <span>${this.escapeHtml(displayName)}</span>
                ${blueDotHtml}
              </div>
              <div class="${msgClass}">
                ${c.last_message_time ? `${this.escapeHtml(lastSenderLabel)}: ` : ''}${this.escapeHtml(truncate(messagePreview))}${statusNote}
              </div>
            </div>
            ${this.renderContactOptions(c)}
          </div>
        `;
      }
    }).join('');
  }

  renderContactOptions(contact) {
    const isGroup = contact.itemType === 'group';
    const chatLabel = isGroup ? 'Nhóm' : 'cuộc trò chuyện';
    const wrapperId = `contact-menu-wrap-${isGroup ? 'group' : 'friend'}-${contact.id}`;
    const menuId = `contact-menu-${isGroup ? 'group' : 'friend'}-${contact.id}`;
    return `
      <div class="contact-options-wrap" id="${this.escapeHtml(wrapperId)}" onclick="event.stopPropagation()">
        <button type="button" class="contact-options-trigger" aria-expanded="false" title="Tùy chọn ${chatLabel}" onclick="app.toggleContactOptions(event, this)">•••</button>
        <div class="contact-options-menu" id="${this.escapeHtml(menuId)}"
          data-origin-wrapper-id="${this.escapeHtml(wrapperId)}"
          data-contact-id="${this.escapeHtml(contact.id)}"
          data-contact-type="${isGroup ? 'group' : 'friend'}"
          data-contact-name="${this.escapeHtml(contact.name || '')}"
          data-contact-avatar="${this.escapeHtml(contact.avatar || '')}"
          data-contact-token="${this.escapeHtml(contact.user_token || contact.id)}">
          <button type="button" data-action="mark-unread" onclick="app.handleContactListAction(event)">Đánh dấu chưa đọc</button>
          ${isGroup ? '' : '<button type="button" data-action="profile" onclick="app.handleContactListAction(event)">Xem trang cá nhân</button>'}
          <button type="button" data-action="${contact.is_archived ? 'unarchive' : 'archive'}" onclick="app.handleContactListAction(event)">${contact.is_archived ? 'Bỏ lưu trữ' : 'Lưu trữ tin nhắn'}</button>
          ${isGroup ? '' : '<button type="button" data-action="block" class="danger" onclick="app.handleContactListAction(event)">Chặn</button><button type="button" data-action="report" class="danger" onclick="app.handleContactListAction(event)">Báo cáo</button>'}
          <button type="button" data-action="delete" class="danger" onclick="app.handleContactListAction(event)">Xóa đoạn chat</button>
        </div>
      </div>
    `;
  }

  closeContactOptionsMenus() {
    document.querySelectorAll('.contact-options-menu').forEach(menu => {
      menu.classList.remove('open');
      menu.classList.remove('is-floating');
      menu.style.removeProperty('position');
      menu.style.removeProperty('top');
      menu.style.removeProperty('left');
      menu.style.removeProperty('right');
      const wrapper = document.getElementById(menu.dataset.originWrapperId);
      const trigger = wrapper?.querySelector('.contact-options-trigger');
      trigger?.classList.remove('is-open');
      trigger?.setAttribute('aria-expanded', 'false');
      if (wrapper && menu.parentElement !== wrapper) wrapper.append(menu);
    });
  }

  toggleContactOptions(event, button) {
    event.stopPropagation();
    const menu = button?.nextElementSibling;
    const wasOpen = menu?.classList.contains('open');
    this.closeContactOptionsMenus();
    this.closeChatListMenu();
    if (!wasOpen && menu) {
      button.classList.add('is-open');
      button.setAttribute('aria-expanded', 'true');
      const triggerRect = button.getBoundingClientRect();
      menu.classList.add('open', 'is-floating');
      document.body.append(menu);
      menu.style.position = 'fixed';
      menu.style.top = '0px';
      menu.style.right = '0px';
      const menuRect = menu.getBoundingClientRect();
      const left = Math.max(8, Math.min(triggerRect.right - menuRect.width, window.innerWidth - menuRect.width - 8));
      const top = triggerRect.bottom + menuRect.height + 8 <= window.innerHeight
        ? triggerRect.bottom + 4
        : Math.max(8, triggerRect.top - menuRect.height - 4);
      menu.style.top = `${top}px`;
      menu.style.left = `${left}px`;
      menu.style.right = 'auto';
    }
  }

  closeChatListMenu() {
    const menu = document.getElementById('chatListMenu');
    if (!menu) return;
    menu.classList.remove('open', 'is-floating');
    menu.style.removeProperty('position');
    menu.style.removeProperty('top');
    menu.style.removeProperty('left');
    menu.style.removeProperty('right');
    const wrapper = document.getElementById(menu.dataset.originWrapperId);
    if (wrapper && menu.parentElement !== wrapper) wrapper.append(menu);
  }

  toggleChatListMenu(event) {
    event.stopPropagation();
    const menu = document.getElementById('chatListMenu');
    const wasOpen = menu?.classList.contains('open');
    this.closeContactOptionsMenus();
    this.closeChatListMenu();
    if (!wasOpen && menu) {
      const wrapper = event.currentTarget.closest('.chat-list-menu-wrap');
      if (wrapper && !wrapper.id) wrapper.id = 'chat-list-menu-wrap-origin';
      menu.dataset.originWrapperId = wrapper?.id || '';
      const triggerRect = event.currentTarget.getBoundingClientRect();
      menu.classList.add('open');
      menu.classList.add('is-floating');
      document.body.append(menu);
      menu.style.position = 'fixed';
      menu.style.top = '0px';
      menu.style.right = '0px';
      const menuRect = menu.getBoundingClientRect();
      const left = Math.max(8, Math.min(triggerRect.right - menuRect.width, window.innerWidth - menuRect.width - 8));
      const top = triggerRect.bottom + menuRect.height + 8 <= window.innerHeight
        ? triggerRect.bottom + 4
        : Math.max(8, triggerRect.top - menuRect.height - 4);
      menu.style.top = `${top}px`;
      menu.style.left = `${left}px`;
      menu.style.right = 'auto';
    }
  }

  toggleArchivedChats() {
    const nextFilter = this.currentContactFilter === 'archived' ? 'all' : 'archived';
    this.currentContactFilter = nextFilter;
    this.closeChatListMenu();
    document.querySelectorAll('.chat-filter-pill').forEach(pill => pill.classList.remove('active'));
    const title = document.querySelector('.chat-list-header-actions')?.parentElement?.querySelector('span');
    if (title) title.textContent = nextFilter === 'archived' ? 'Đoạn chat lưu trữ' : 'Đoạn Chat';
    this.renderContactsList(nextFilter);
  }

  async toggleBlockedUsers() {
    this.closeChatListMenu();
    if (this.currentContactFilter === 'blocked') {
      const allButton = document.querySelector('.chat-filter-pill');
      this.filterContacts('all', allButton);
      return;
    }
    if (!this.currentUser) return;
    this.currentContactFilter = 'blocked';
    const title = document.querySelector('.chat-list-header-actions')?.parentElement?.querySelector('span');
    if (title) title.textContent = 'Tài khoản đã bị chặn';
    document.querySelectorAll('.chat-filter-pill').forEach(pill => pill.classList.remove('active'));
    const container = document.getElementById('contactsContainer');
    if (container) container.innerHTML = '<div class="all-message-search-empty">Đang tải danh sách bị chặn...</div>';
    try {
      const response = await fetch(`/api/users/blocked?user_id=${encodeURIComponent(this.currentUser.id)}`);
      const data = await this.readApiResponse(response);
      if (!response.ok || data.status !== 'ok') throw new Error(data.message || 'Không thể tải danh sách bị chặn.');
      const users = data.users || [];
      if (!users.length) {
        if (container) container.innerHTML = '<div class="all-message-search-empty">Không có tài khoản nào bị chặn.</div>';
        return;
      }
      if (container) {
        container.innerHTML = users.map(u => `
          <div class="contact-item blocked-user-item" data-contact-id="${this.escapeHtml(u.id)}">
            <div class="contact-avatar-box">
              <img src="${this.escapeHtml(u.avatar || '/img/default-avatar.svg')}" alt="Avatar" onerror="this.src='/img/default-avatar.svg'">
            </div>
            <div class="contact-info" style="flex:1;min-width:0;">
              <div class="contact-name">${this.escapeHtml(u.name || 'Người dùng')}</div>
              <div class="contact-last-msg read-msg">Đã chặn${u.blocked_at ? ' · ' + this.formatTimeAgo(u.blocked_at) : ''}</div>
            </div>
            <button type="button" class="btn-sm" style="flex-shrink:0;padding:6px 10px;border-radius:8px;border:1px solid var(--border);background:var(--bg-secondary);cursor:pointer;font-size:0.78rem;"
              onclick="event.stopPropagation(); app.unblockUser(${app.jsArg(u.id)}, ${app.jsArg(u.name || '')})">Gỡ chặn</button>
          </div>
        `).join('');
      }
    } catch (error) {
      if (container) container.innerHTML = '<div class="all-message-search-empty">Không thể tải danh sách bị chặn.</div>';
      this.showToast(error.message || 'Không thể tải danh sách bị chặn.', 'error');
    }
  }

  async unblockUser(userId, userName = '') {
    if (!this.currentUser || !userId) return;
    const confirmed = await this.showConfirm(`Gỡ chặn ${userName || 'người dùng này'}?`);
    if (!confirmed) return;
    try {
      const response = await fetch('/api/users/unblock', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ my_id: this.currentUser.id, target_id: userId })
      });
      const data = await this.readApiResponse(response);
      if (!response.ok || data.status !== 'ok') throw new Error(data.message || 'Không thể gỡ chặn.');
      this.showToast('Đã gỡ chặn người dùng.', 'success');
      if (typeof this.toggleBlockedUsers === 'function') {
        try { await this.toggleBlockedUsers(); } catch (_) {}
      }
      if (document.getElementById('blockedUsersModalList')) {
        await this.renderBlockedUsersModalList();
      }
      if (typeof this.loadFriendsAndContacts === 'function') {
        try { await this.loadFriendsAndContacts(); } catch (_) {}
      }
      // Đang xem hồ sơ người vừa gỡ chặn: tải lại để nút chuyển về "Thêm bạn bè" (hoặc "Bạn bè" nếu đã là bạn)
      if (document.getElementById('profileActionsRow') && /profile\.html/i.test(window.location.pathname)) {
        try { await this.initProfilePage(userId); } catch (_) {}
      }
    } catch (error) {
      this.showToast(error.message || 'Không thể gỡ chặn.', 'error');
    }
  }

  async togglePendingMessages() {
    this.closeChatListMenu();
    if (this.currentContactFilter === 'pending') {
      const allButton = document.querySelector('.chat-filter-pill');
      this.filterContacts('all', allButton);
      return;
    }
    if (!this.currentUser) return;
    this.currentContactFilter = 'pending';
    const title = document.querySelector('.chat-list-header-actions')?.parentElement?.querySelector('span');
    if (title) title.textContent = 'Tin nhắn đang chờ';
    document.querySelectorAll('.chat-filter-pill').forEach(pill => pill.classList.remove('active'));
    const container = document.getElementById('contactsContainer');
    if (container) container.innerHTML = '<div class="all-message-search-empty">Đang tải tin nhắn đang chờ...</div>';

    try {
      const response = await fetch(`/api/messages/pending?user_id=${encodeURIComponent(this.currentUser.id)}`);
      const data = await this.readApiResponse(response);
      if (!response.ok || data.status !== 'ok') {
        throw new Error(data.message || 'Không thể tải tin nhắn đang chờ.');
      }
      this.pendingMessageContacts = (data.contacts || []).map(contact => ({
        ...contact,
        itemType: 'pending'
      }));
      if (this.currentContactFilter === 'pending') this.renderContactsList('pending');
    } catch (error) {
      if (container) container.innerHTML = '<div class="all-message-search-empty">Không thể tải tin nhắn đang chờ.</div>';
      this.showToast(error.message || 'Không thể tải tin nhắn đang chờ.', 'error');
    }
  }

  showChatHelp() {
    this.closeChatListMenu();
    this.showToast('Dùng dấu ba chấm cạnh cuộc trò chuyện để đánh dấu chưa đọc, xem hồ sơ, chặn, báo cáo, xóa hoặc lưu trữ.', 'info');
  }

  async updateChatPreference(contact, action) {
    const isGroup = contact.itemType === 'group';
    const response = await fetch('/api/chats/preferences', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        user_id: this.currentUser.id,
        conversation_id: contact.id,
        is_group: isGroup,
        action
      })
    });
    const data = await this.readApiResponse(response);
    if (!response.ok || data.status !== 'ok') throw new Error(data.message || 'Không thể cập nhật đoạn chat.');
  }

  async readApiResponse(response) {
    const body = await response.text();
    if (!body.trim()) {
      throw new Error(`Máy chủ trả phản hồi rỗng (HTTP ${response.status}). Hãy khởi động lại máy chủ đã biên dịch mới nhất.`);
    }
    try {
      return JSON.parse(body);
    } catch (error) {
      console.error('Phản hồi API không phải JSON hợp lệ:', error);
      throw new Error(`Phản hồi máy chủ không hợp lệ (HTTP ${response.status}).`);
    }
  }

  async handleContactListAction(event) {
    event.stopPropagation();
    const action = event.currentTarget?.dataset.action;
    const menu = event.currentTarget?.closest('.contact-options-menu');
    const row = event.currentTarget?.closest('.contact-item');
    const source = row || menu;
    if (!action || !source || !this.currentUser) return;
    const contact = {
      id: source.dataset.contactId,
      itemType: source.dataset.contactType,
      name: source.dataset.contactName || '',
      avatar: source.dataset.contactAvatar || '',
      user_token: source.dataset.contactToken || source.dataset.contactId
    };
    const closeMenus = () => this.closeContactOptionsMenus();
    const updateCache = (key, value) => {
      const source = contact.itemType === 'group' ? this.allContactsCache.groups : this.allContactsCache.friends;
      const cached = source?.find(item => item.id === contact.id);
      if (cached) cached[key] = value;
    };

    try {
      if (action === 'archive' || action === 'unarchive') {
        await this.updateChatPreference(contact, action);
        updateCache('is_archived', action === 'archive');
        this.showToast(action === 'archive' ? 'Đã lưu trữ đoạn chat.' : 'Đã bỏ lưu trữ đoạn chat.', 'success');
        closeMenus();
        this.renderContactsList(this.currentContactFilter || 'all');
      } else if (action === 'mark-unread') {
        await this.updateChatPreference(contact, 'mark_unread');
        updateCache('is_unread', true);
        updateCache('force_unread', true);
        closeMenus();
        this.renderContactsList(this.currentContactFilter || 'all');
      } else if (action === 'profile') {
        closeMenus();
        this.openUserProfile(contact.user_token);
      } else if (action === 'block') {
        const confirmed = await this.showConfirm(`Chặn ${contact.name}?`);
        if (!confirmed) return;
        const response = await fetch('/api/users/block', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ my_id: this.currentUser.id, target_id: contact.id })
        });
        const data = await this.readApiResponse(response);
        if (!response.ok || data.status !== 'ok') throw new Error(data.message || 'Không thể chặn người dùng.');
        closeMenus();
        if (this.currentChatPartner?.id === contact.id) this.closeChat();
        this.showToast('Đã chặn người dùng.', 'success');
        await this.loadFriendsAndContacts();
      } else if (action === 'report') {
        closeMenus();
        this.openReportModal(contact.id, '');
      } else if (action === 'delete') {
        const prompt = contact.itemType === 'group'
          ? 'Xóa toàn bộ tin nhắn nhóm chỉ khỏi danh sách của bạn?'
          : 'Xóa toàn bộ tin nhắn với người này ở cả hai phía?';
        const confirmed = await this.showConfirm(prompt);
        if (!confirmed) return;
        if (contact.itemType === 'group') {
          const response = await fetch('/api/groups/messages/clear-for-me', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ group_id: contact.id, user_id: this.currentUser.id })
          });
          const data = await this.readApiResponse(response);
          if (!response.ok || data.status !== 'ok') throw new Error(data.message || 'Không thể xóa đoạn chat nhóm.');
        } else {
          const response = await fetch('/api/messages/clear', {
            method: 'DELETE',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ my_id: this.currentUser.id, partner_id: contact.id })
          });
          const data = await this.readApiResponse(response);
          if (!response.ok || data.status !== 'ok') throw new Error(data.message || 'Không thể xóa đoạn chat.');
        }
        closeMenus();
        this.showToast('Đã xóa đoạn chat.', 'success');
        if (this.currentChatPartner?.id === contact.id) {
          if (contact.itemType === 'group') await this.loadGroupMessages(contact.id);
          else await this.loadInitialMessages();
        }
        await this.loadFriendsAndContacts();
      }
    } catch (error) {
      this.showToast(error.message || 'Không thể hoàn tất thao tác.', 'error');
    }
  }

  async openChatWith(partnerId, partnerName, partnerAvatar, partnerToken = '') {
    this.resetMessageReadTracking();
    this.msgCache = {};
    this.exitHistoryView(false);
    this.resetChatSearchState();
    const messagesList = document.getElementById('messagesList');
    if (messagesList) {
      messagesList.style.paddingTop = '';
      messagesList.style.paddingBottom = '';
    }
    this.currentChatPartner = { id: partnerId, name: partnerName, avatar: partnerAvatar, user_token: partnerToken || partnerId, isGroup: false };
    const knownFriend = this.allContactsCache?.friends?.some(contact => contact.id === partnerId);
    if (knownFriend) {
      this.updateChatPreference({ id: partnerId, itemType: 'friend' }, 'mark_read')
        .then(() => {
          const contact = this.allContactsCache?.friends?.find(item => item.id === partnerId);
          if (contact) {
            contact.is_unread = false;
            contact.force_unread = false;
            contact.unread_count = 0;
            this.renderContactsList(this.currentContactFilter || 'all');
          }
        })
        .catch(error => console.error('Không thể đánh dấu cuộc trò chuyện đã đọc:', error));
    }
    document.body.classList.add('has-open-chat');
    const emptyState = document.getElementById('chatEmptyState');
    if (emptyState) emptyState.style.display = 'none';
    document.getElementById('chatPartnerName').textContent = partnerName;
    document.getElementById('chatPartnerAvatar').src = partnerAvatar || '/img/default-avatar.svg';
    document.getElementById('chatWindow').style.display = 'flex';
    this.clearPendingChatMedia();
    this.cancelChatReply();

    // Đánh dấu người này đã đọc ngay trên giao diện danh bạ (dấu chấm xanh biến mất, chữ chuyển sang nhạt)
    if (this.allContactsCache && this.allContactsCache.friends) {
      const targetFriend = this.allContactsCache.friends.find(f => f.id === partnerId);
      if (targetFriend && targetFriend.is_unread) {
        targetFriend.is_unread = false;
        targetFriend.unread_count = 0;
        this.renderContactsList(this.currentContactFilter || 'all');
      }
    }

    // Cập nhật trạng thái hoạt động (Đang hoạt động / x phút trước - Hình 3)
    this.updateChatHeaderStatus();

    // Kiểm tra biệt danh nếu có (Item 10)
    try {
      const res = await fetch(`/api/friends/nickname?user_id=${this.currentUser.id}&friend_id=${partnerId}`);
      const data = await res.json();
      if (data.status === 'ok' && data.nickname) {
        document.getElementById('chatPartnerName').textContent = data.nickname;
      }
    } catch (e) {}

    await this.loadInitialMessages();
    // Bắt đầu typing indicator, read receipt & message polling theo thời gian thực
    this.startTypingIndicator();
    this.startTypingPoll();
    this.startReadReceiptPoll();
    this.startMessagePolling();
  }

  async openGroupChat(groupId, groupName, groupAvatar = '') {
    this.resetMessageReadTracking();
    const gAvatar = groupAvatar || '/img/default-avatar.svg';
    this.msgCache = {};
    this.exitHistoryView(false);
    this.resetChatSearchState();
    const messagesList = document.getElementById('messagesList');
    if (messagesList) {
      messagesList.style.paddingTop = '';
      messagesList.style.paddingBottom = '';
    }
    this.currentChatPartner = { id: groupId, name: groupName, avatar: gAvatar, isGroup: true, role: 'MEMBER' };
    this.updateChatPreference({ id: groupId, itemType: 'group' }, 'mark_read')
      .then(() => {
        const group = this.allContactsCache?.groups?.find(item => item.id === groupId);
        if (group) {
          group.is_unread = false;
          this.renderContactsList(this.currentContactFilter || 'all');
        }
      })
      .catch(error => console.error('Không thể đánh dấu nhóm đã đọc:', error));
    document.body.classList.add('has-open-chat');
    const emptyState = document.getElementById('chatEmptyState');
    if (emptyState) emptyState.style.display = 'none';
    document.getElementById('chatPartnerName').textContent = `👥 ${groupName}`;
    document.getElementById('chatPartnerAvatar').src = gAvatar;
    document.getElementById('chatWindow').style.display = 'flex';
    this.clearPendingChatMedia();
    this.cancelChatReply();

    // Cập nhật trạng thái số thành viên nhóm
    this.updateChatHeaderStatus();

    // Kiểm tra vai trò của người dùng trong nhóm
    try {
      const res = await fetch(`/api/groups/members?group_id=${encodeURIComponent(groupId)}`);
      const data = await res.json();
      if (data.status === 'ok' && data.members) {
        this.currentChatPartner.member_count = data.members.length;
        const me = data.members.find(m => m.id === this.currentUser.id);
        if (me) this.currentChatPartner.role = me.role;
        this.updateChatHeaderStatus();
      }
    } catch (e) {}

    await this.loadGroupMessages(groupId);
    this.startMessagePolling();
  }

  closeChat() {
    this.exitHistoryView(false);
    this.resetChatSearchState();
    const chatWindow = document.getElementById('chatWindow');
    const isMessengerPage = document.body.classList.contains('messenger-page');
    if (chatWindow) chatWindow.style.display = isMessengerPage ? 'flex' : 'none';
    document.body.classList.remove('has-open-chat');
    const emptyState = document.getElementById('chatEmptyState');
    if (emptyState) emptyState.style.display = '';
    this.clearPendingChatMedia();
    this.stopTyping();
    this.stopTypingPoll();
    this.stopReadReceiptPoll();
    this.stopMessagePolling();
    this.cancelChatReply();
    this.closeAllMessageDropdowns();
    this.currentChatPartner = null;
  }

  // --- TRẠNG THÁI HOẠT ĐỘNG (Hình 3) ---
  formatActivityStatus(isOnline, lastActiveAt) {
    if (isOnline) return 'Đang hoạt động';
    if (!lastActiveAt) return '';
    const t = new Date(lastActiveAt.replace(' ', 'T')).getTime();
    if (isNaN(t)) return '';
    const diffSec = Math.floor((Date.now() - t) / 1000);
    const diffMin = Math.max(1, Math.floor(diffSec / 60));
    if (diffMin < 60) return `${diffMin} phút trước`;
    const diffHour = Math.floor(diffMin / 60);
    if (diffHour < 24) return `${diffHour} giờ trước`;
    return ''; // Nếu trên 1 ngày không hoạt động thì không hiển thị gì cả
  }

  async updateChatHeaderStatus() {
    if (!this.currentChatPartner) return;
    const statusEl = document.getElementById('chatPartnerStatus');
    if (!statusEl) return;

    if (this.currentChatPartner.isGroup) {
      statusEl.style.display = 'block';
      statusEl.style.color = 'var(--text-muted)';
      const mCount = this.currentChatPartner.member_count;
      statusEl.textContent = mCount ? `${mCount} thành viên` : 'Nhóm trò chuyện';
      return;
    }

    try {
      const res = await fetch(`/api/users/profile?id=${encodeURIComponent(this.currentChatPartner.id)}&my_id=${this.currentUser.id}`);
      const data = await res.json();
      if (data.status === 'ok' && data.user) {
        const u = data.user;
        const statusText = this.formatActivityStatus(u.is_online, u.last_active_at);
        if (statusText === 'Đang hoạt động') {
          statusEl.textContent = 'Đang hoạt động';
          statusEl.style.color = 'var(--accent-green)';
          statusEl.style.display = 'block';
        } else if (statusText) {
          statusEl.textContent = statusText;
          statusEl.style.color = 'var(--text-muted)';
          statusEl.style.display = 'block';
        } else {
          statusEl.textContent = '';
          statusEl.style.display = 'none';
        }
      }
    } catch (e) {}
  }

  // --- REAL-TIME MESSAGE POLLING (Hiển thị tin nhắn mới theo thời gian thực) ---
  startMessagePolling() {
    this.stopMessagePolling();
    this.messagePollInterval = setInterval(async () => {
      if (!this.currentChatPartner) return;
      const chatWin = document.getElementById('chatWindow');
      if (!chatWin || chatWin.style.display === 'none') return;
      // Đang xem ngữ cảnh tin nhắn cũ (sau khi nhảy tới tin gốc): không render đè lên
      if (this.isViewingHistory) return;

      const isGroup = this.currentChatPartner.isGroup;
      const url = isGroup
        ? `/api/groups/messages?group_id=${encodeURIComponent(this.currentChatPartner.id)}&viewer_id=${this.currentUser ? this.currentUser.id : ''}&limit=30`
        : `/api/messages?my_id=${this.currentUser.id}&partner_id=${this.currentChatPartner.id}&limit=30&before_id=0`;

      try {
        const res = await fetch(url);
        const data = await res.json();
        if (data.status === 'ok' && Array.isArray(data.messages)) {
          const signature = data.messages.map(m => `${m.id}_${m.is_revoked}_${m.reaction || ''}_${JSON.stringify(m.reactions || [])}_${JSON.stringify(m.receipts || [])}_${m.content}_${m.is_read || 0}`).join('|');
          if (signature !== this.lastRenderedMsgSignature) {
            this.lastRenderedMsgSignature = signature;
            const listEl = document.getElementById('messagesList');
            const chatBody = document.getElementById('chatBody');
            const isNearBottom = chatBody ? (chatBody.scrollHeight - chatBody.scrollTop - chatBody.clientHeight < 160) : true;

            listEl.innerHTML = this.renderMessagesHtml(data.messages);

            if (this.messageScrollAnchor) {
              this.restoreMessageScrollAnchor(this.messageScrollAnchor);
            } else if (isNearBottom && chatBody) {
              chatBody.scrollTop = chatBody.scrollHeight;
            }
            this.updateReadReceipt();
            this.loadFriendsAndContacts();
          }
        }
      } catch (e) {}

      this.updateChatHeaderStatus();
    }, 1500);
  }

  stopMessagePolling() {
    if (this.messagePollInterval) {
      clearInterval(this.messagePollInterval);
      this.messagePollInterval = null;
    }
  }

  toggleChatMenu() {
    const d = document.getElementById('chatMoreDropdown');
    if (!d) return;
    const isShowing = (d.style.display === 'block');
    if (isShowing) {
      d.style.display = 'none';
      return;
    }
    const isGroup = this.currentChatPartner && this.currentChatPartner.isGroup;

    // Phân biệt menu giữa Nhóm và Bạn bè (Item 13)
    const setDisplay = (id, show) => {
      const el = document.getElementById(id);
      if (el) el.style.display = show ? 'block' : 'none';
    };

    setDisplay('chatMenuNicknameBtn', !isGroup);
    setDisplay('chatMenuGroupBtn', !isGroup);
    setDisplay('chatMenuBlockBtn', !isGroup);

    setDisplay('chatMenuRenameGroupBtn', isGroup);
    setDisplay('chatMenuMembersBtn', isGroup);
    setDisplay('chatMenuAddMemberBtn', isGroup);
    setDisplay('chatMenuChangeAvatarBtn', isGroup);
    setDisplay('chatMenuLeaveBtn', isGroup);

    // Xóa thành viên chỉ hiển thị nếu là trưởng nhóm (role == 'ADMIN')
    if (isGroup && this.currentChatPartner.role === 'ADMIN') {
      setDisplay('chatMenuRemoveMemberBtn', true);
    } else {
      setDisplay('chatMenuRemoveMemberBtn', false);
    }

    d.style.display = 'block';
  }

  chatActionQuickEmoji() {
    const menu = document.getElementById('chatMoreDropdown');
    if (menu) menu.style.display = 'none';
    this.openChatEmojiModal('quick-reactions-edit');
  }

  // --- TẢI MEDIA TRONG CHAT (HÌNH ẢNH & VIDEO) (Item 1) ---
  handleChatMediaUpload(input) {
    if (!input.files || input.files.length === 0) return;
    const file = input.files[0];
    const isImage = file.type.startsWith('image/');
    const isVideo = file.type.startsWith('video/');
    if (!isImage && !isVideo) {
      this.showToast('Chỉ chấp nhận tệp hình ảnh hoặc video!', 'error');
      input.value = '';
      return;
    }
    if (!this.isMediaFileWithinLimit(file, input)) return;

    const reader = new FileReader();
    reader.onload = (e) => {
      this.pendingChatMedia = {
        type: isImage ? 'IMAGE' : 'VIDEO',
        url: e.target.result,
        name: file.name
      };
      this.showChatMediaPreview();
    };
    reader.readAsDataURL(file);
    input.value = '';
  }

  showChatMediaPreview() {
    let previewBox = document.getElementById('chatMediaPreviewBox');
    if (!previewBox) {
      previewBox = document.createElement('div');
      previewBox.id = 'chatMediaPreviewBox';
      previewBox.style.cssText = 'display:flex; align-items:center; gap:8px; padding:6px 12px; background:var(--bg-surface-secondary); border-top:1px solid var(--border-color);';
      const footer = document.querySelector('.chat-footer');
      if (footer) footer.parentNode.insertBefore(previewBox, footer);
    }

    if (!this.pendingChatMedia) {
      previewBox.style.display = 'none';
      return;
    }

    const mediaThumb = this.pendingChatMedia.type === 'IMAGE'
      ? `<img src="${this.pendingChatMedia.url}" style="width:36px; height:36px; object-fit:cover; border-radius:4px;">`
      : `<span style="font-size:1.5rem;">🎥</span>`;

    previewBox.style.display = 'flex';
    previewBox.innerHTML = `
      ${mediaThumb}
      <span style="font-size:0.8rem; color:var(--text-primary); flex:1; overflow:hidden; text-overflow:ellipsis; white-space:nowrap;">${this.escapeHtml(this.pendingChatMedia.name || 'Tệp đính kèm')}</span>
      <button class="btn-icon" style="width:24px; height:24px;" onclick="app.clearPendingChatMedia()">✕</button>
    `;
  }

  clearPendingChatMedia() {
    this.pendingChatMedia = null;
    const box = document.getElementById('chatMediaPreviewBox');
    if (box) box.style.display = 'none';
  }

  // --- TÌM KIẾM TIN NHẮN TRONG ĐOẠN CHAT (không phân biệt hoa/thường, dấu) ---
  chatActionSearchMessages() {
    this.toggleChatMenu();
    const bar = document.getElementById('chatSearchBar');
    if (!bar) return;
    bar.style.display = (bar.style.display === 'none' || !bar.style.display) ? 'block' : 'none';
    if (bar.style.display === 'block') {
      const inp = document.getElementById('chatSearchInput');
      if (inp) { inp.value = ''; inp.focus(); }
      const dd = document.getElementById('chatSearchResultsDropdown');
      if (dd) { dd.style.display = 'none'; dd.innerHTML = ''; }
      this.resetChatSearchState(false);
    } else {
      this.searchChatMessages('');
    }
  }

  searchChatMessages(term) {
    clearTimeout(this._chatSearchDebounce);
    const q = (term || '').trim();
    const dropdown = document.getElementById('chatSearchResultsDropdown');
    if (!dropdown) return;
    if (!this.chatSearchScrollBound) {
      dropdown.addEventListener('scroll', () => {
        if (dropdown.scrollTop + dropdown.clientHeight >= dropdown.scrollHeight - 48) {
          this.loadMoreChatSearchMessages();
        }
      }, { passive: true });
      this.chatSearchScrollBound = true;
    }

    // Hiện lại tất cả tin nhắn đang hiển thị (bỏ filter client-side cũ)
    document.querySelectorAll('#messagesList .message-row').forEach(r => { r.style.display = 'flex'; });

    this.chatSearchRequestId += 1;
    const requestId = this.chatSearchRequestId;
    this.chatSearchResults = [];
    this.chatSearchOffset = 0;
    this.chatSearchHasMore = false;
    this.chatSearchLoading = false;
    if (!q) {
      dropdown.style.display = 'none';
      dropdown.innerHTML = '';
      return;
    }
    if (!this.currentChatPartner || !this.currentUser) {
      dropdown.style.display = 'none';
      return;
    }

    dropdown.innerHTML = '<div class="chat-search-item chat-search-loading">Đang tìm tin nhắn...</div>';
    dropdown.style.display = 'block';
    this._chatSearchDebounce = setTimeout(() => this.fetchChatSearchMessages(q, requestId), 250);
  }

  async fetchChatSearchMessages(query, requestId, append = false) {
    const dropdown = document.getElementById('chatSearchResultsDropdown');
    if (!dropdown || !this.currentChatPartner || !this.currentUser || requestId !== this.chatSearchRequestId) return;
    if (this.chatSearchLoading || (append && !this.chatSearchHasMore)) return;
    const previousScrollTop = append ? dropdown.scrollTop : 0;
    this.chatSearchLoading = true;
    const offset = append ? this.chatSearchOffset : 0;
    const isGroup = !!this.currentChatPartner.isGroup;
    try {
      const url = `/api/messages/search?my_id=${encodeURIComponent(this.currentUser.id)}&partner_id=${encodeURIComponent(this.currentChatPartner.id)}&q=${encodeURIComponent(query)}&is_group=${isGroup ? 'true' : 'false'}&limit=10&offset=${offset}`;
      const res = await fetch(url);
      const data = await res.json();
      if (requestId !== this.chatSearchRequestId) return;
      if (!res.ok || data.status !== 'ok') throw new Error(data.message || 'Không thể tìm tin nhắn.');
      const messages = Array.isArray(data.messages) ? data.messages : [];
      this.chatSearchResults = append ? [...this.chatSearchResults, ...messages] : messages;
      this.chatSearchOffset = this.chatSearchResults.length;
      this.chatSearchHasMore = !!data.has_more;
      this.chatSearchLoading = false;
      this.renderChatSearchResults(query);
      if (append) dropdown.scrollTop = previousScrollTop;
    } catch (error) {
      console.error('Lỗi tìm kiếm tin nhắn:', error);
      if (requestId === this.chatSearchRequestId) {
        if (append && this.chatSearchResults.length) {
          this.renderChatSearchResults(query);
          dropdown.insertAdjacentHTML('beforeend', `<div class="chat-search-item chat-search-error">${this.escapeHtml(error.message || 'Lỗi tìm kiếm tin nhắn.')}</div>`);
        } else {
          dropdown.innerHTML = `<div class="chat-search-item chat-search-error">${this.escapeHtml(error.message || 'Lỗi tìm kiếm tin nhắn.')}</div>`;
        }
        dropdown.style.display = 'block';
      }
    } finally {
      if (requestId === this.chatSearchRequestId) this.chatSearchLoading = false;
    }
  }

  loadMoreChatSearchMessages() {
    const query = document.getElementById('chatSearchInput')?.value.trim();
    if (!query || !this.chatSearchHasMore || this.chatSearchLoading) return;
    const requestId = this.chatSearchRequestId;
    const dropdown = document.getElementById('chatSearchResultsDropdown');
    if (dropdown && !dropdown.querySelector('.chat-search-loading-more')) {
      dropdown.insertAdjacentHTML('beforeend', '<div class="chat-search-item chat-search-loading-more">Đang tải thêm...</div>');
    }
    this.fetchChatSearchMessages(query, requestId, true);
  }

  resetChatSearchState(clearInput = true) {
    clearTimeout(this._chatSearchDebounce);
    this.chatSearchRequestId += 1;
    this.chatSearchResults = [];
    this.chatSearchOffset = 0;
    this.chatSearchHasMore = false;
    this.chatSearchLoading = false;
    if (clearInput) {
      const input = document.getElementById('chatSearchInput');
      if (input) input.value = '';
    }
    const dropdown = document.getElementById('chatSearchResultsDropdown');
    if (dropdown) {
      dropdown.innerHTML = '';
      dropdown.style.display = 'none';
    }
  }

  renderChatSearchResults(query) {
    const dropdown = document.getElementById('chatSearchResultsDropdown');
    if (!dropdown) return;
    if (!this.chatSearchResults.length) {
      dropdown.innerHTML = '<div class="chat-search-item chat-search-empty">Không tìm thấy tin nhắn phù hợp</div>';
      dropdown.style.display = 'block';
      return;
    }
    const esc = value => this.escapeHtml(value || '');
    const highlightSnippet = content => {
      const raw = content || '';
      const normalized = this.removeVietnameseTonesClient(raw);
      const needle = this.removeVietnameseTonesClient(query);
      const index = normalized.indexOf(needle);
      if (index < 0) return esc(raw.length > 80 ? raw.slice(0, 80) + '…' : raw);
      const start = Math.max(0, index - 20);
      const end = Math.min(raw.length, index + query.length + 40);
      let snippet = raw.slice(start, end);
      if (start > 0) snippet = '…' + snippet;
      if (end < raw.length) snippet += '…';
      const escaped = esc(snippet);
      try {
        const expression = new RegExp(`(${query.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')})`, 'gi');
        return escaped.replace(expression, '<span class="chat-search-match-highlight">$1</span>');
      } catch (error) {
        console.error('Không thể làm nổi bật kết quả tìm kiếm:', error);
        return escaped;
      }
    };
    const resultHtml = this.chatSearchResults.map(message => `
      <div class="chat-search-item" data-msg-id="${message.id}"
           onclick="app.jumpToChatMessage(${message.id}, event)"
           title="Nhấn để chuyển tới tin nhắn">
        <div class="chat-search-item-header">
          <span class="chat-search-item-sender">${esc(message.sender_name || 'Người dùng')}</span>
          <span>${this.formatTimeAgo(message.created_at)}</span>
        </div>
        <div class="chat-search-item-snippet">${highlightSnippet(message.content)}</div>
      </div>`).join('');
    const loading = this.chatSearchLoading ? '<div class="chat-search-item chat-search-loading-more">Đang tải thêm...</div>' : '';
    dropdown.innerHTML = resultHtml + loading;
    dropdown.style.display = 'block';
  }

  startMessageTimestampTimer(msgId) {
    this.cancelMessageTimestampTimer(msgId);
    const row = document.getElementById(`msg-${msgId}`);
    if (!row) return;
    const timestamp = row.querySelector('.message-hover-timestamp');
    if (timestamp) {
      timestamp.style.removeProperty('max-width');
      timestamp.style.removeProperty('overflow');
      timestamp.style.removeProperty('text-overflow');
      timestamp.style.removeProperty('left');
      timestamp.style.removeProperty('right');
    }
    const timer = window.setTimeout(() => {
      const container = timestamp?.parentElement;
      const chatBody = row.closest('.chat-body');
      if (timestamp && container && chatBody) {
        const containerRect = container.getBoundingClientRect();
        const chatBodyRect = chatBody.getBoundingClientRect();
        const availableSpace = row.classList.contains('me')
          ? containerRect.left - chatBodyRect.left
          : chatBodyRect.right - containerRect.right;
        const timestampWidth = timestamp.getBoundingClientRect().width;

        if (availableSpace < timestampWidth + 8) {
          timestamp.style.maxWidth = `${Math.max(20, containerRect.width - 12)}px`;
          timestamp.style.overflow = 'hidden';
          timestamp.style.textOverflow = 'ellipsis';
          if (row.classList.contains('me')) {
            timestamp.style.left = '6px';
            timestamp.style.right = 'auto';
          } else {
            timestamp.style.left = 'auto';
            timestamp.style.right = '6px';
          }
        }
      }
      row.classList.add('show-message-timestamp');
      this.messageTimestampTimers.delete(msgId);
    }, 1000);
    this.messageTimestampTimers.set(msgId, timer);
  }

  cancelMessageTimestampTimer(msgId) {
    const timer = this.messageTimestampTimers.get(msgId);
    if (timer) window.clearTimeout(timer);
    this.messageTimestampTimers.delete(msgId);
    const row = document.getElementById(`msg-${msgId}`);
    row?.classList.remove('show-message-timestamp');
  }

  searchAllMessages(term) {
    clearTimeout(this._allMessagesSearchDebounce);
    const query = (term || '').trim();
    const results = document.getElementById('allMessagesSearchResults');
    const searchBox = results?.closest('.all-message-search');
    if (!results || !searchBox) return;
    searchBox.classList.toggle('searching', !!query);
    if (!query || !this.currentUser) {
      this._allMessageSearchQuery = '';
      this._allMessageSearchData = null;
      results.innerHTML = '';
      results.style.display = 'none';
      this.allMessageSearchFilter = 'all';
      document.querySelectorAll('#allMessageSearchFilters button').forEach(button => {
        button.classList.toggle('active', button.dataset.filter === 'all');
      });
      return;
    }

    this._allMessageSearchQuery = query;
    results.innerHTML = '<div class="all-message-search-empty">Đang tìm tin nhắn...</div>';
    results.style.display = 'block';
    this._allMessagesSearchDebounce = setTimeout(async () => {
      try {
        const url = `/api/messages/search-all?user_id=${encodeURIComponent(this.currentUser.id)}&q=${encodeURIComponent(query)}`;
        const response = await fetch(url);
        const data = await response.json();
        if (!response.ok || data.status !== 'ok') {
          throw new Error(data.message || 'Không thể tìm tin nhắn.');
        }
        if (this._allMessageSearchQuery !== query) return;
        this._allMessageSearchData = {
          query,
          messages: Array.isArray(data.messages) ? data.messages : []
        };
        this.renderAllMessageSearchResults();
      } catch (error) {
        console.error('Lỗi tìm kiếm tin nhắn toàn cục:', error);
        results.innerHTML = '<div class="all-message-search-empty">Không thể tìm tin nhắn lúc này.</div>';
        results.style.display = 'block';
      }
    }, 220);
  }

  setAllMessageSearchFilter(filter, button) {
    this.allMessageSearchFilter = filter;
    document.querySelectorAll('#allMessageSearchFilters button').forEach(item => item.classList.remove('active'));
    if (button) button.classList.add('active');
    this.renderAllMessageSearchResults();
  }

  renderAllMessageSearchResults() {
    const results = document.getElementById('allMessagesSearchResults');
    const data = this._allMessageSearchData;
    if (!results || !data) return;
    const query = this.removeVietnameseTonesClient(data.query.trim());
    const includesQuery = value => this.removeVietnameseTonesClient(String(value || '')).includes(query);
    const contacts = this.allContactsCache || { friends: [], groups: [] };
    const people = (contacts.friends || []).filter(friend =>
      includesQuery(friend.name) || includesQuery(friend.real_name) || includesQuery(friend.nickname)
    ).slice(0, 10);
    const groups = (contacts.groups || []).filter(group => includesQuery(group.name)).slice(0, 10);
    const messages = data.messages || [];
    const filter = this.allMessageSearchFilter || 'all';
    const groupMessages = messages.filter(message => message.is_group);
    const directMessages = messages.filter(message => !message.is_group);

    const renderContact = (contact, isGroup) => {
      const name = isGroup ? contact.name : (contact.nickname || contact.name);
      return `
        <button type="button" class="all-message-search-result"
          data-result-kind="contact" data-is-group="${isGroup ? 'true' : 'false'}"
          data-conversation-id="${this.escapeHtml(contact.id || '')}"
          data-conversation-name="${this.escapeHtml(name || 'Cuộc trò chuyện')}"
          data-conversation-avatar="${this.escapeHtml(contact.avatar || '')}"
          data-conversation-token="${this.escapeHtml(contact.user_token || contact.id || '')}">
          <img src="${this.escapeHtml(contact.avatar || '/img/default-avatar.svg')}" alt="">
          <span class="all-message-search-result-text">
            <span class="all-message-search-result-heading"><strong>${this.escapeHtml(name || 'Cuộc trò chuyện')}</strong></span>
            <span class="all-message-search-result-snippet">${isGroup ? 'Nhóm' : 'Bạn bè'}</span>
          </span>
        </button>`;
    };
    const renderMessage = match => {
      const isGroup = !!match.is_group;
      const conversationId = isGroup ? match.group_id : match.partner_id;
      const conversationName = isGroup ? match.group_name : match.partner_name;
      const conversationAvatar = isGroup ? match.group_avatar : match.partner_avatar;
      const sender = match.sender_id === this.currentUser?.id ? 'Bạn' : (match.sender_name || 'Người gửi');
      const text = match.content || (match.media_type === 'IMAGE'
        ? 'đã gửi Hình ảnh'
        : match.media_type === 'VIDEO' ? 'đã gửi Video' : '[Tin nhắn]');
      return `
        <button type="button" class="all-message-search-result"
          data-result-kind="message" data-message-id="${Number(match.id) || 0}"
          data-is-group="${isGroup ? 'true' : 'false'}"
          data-conversation-id="${this.escapeHtml(conversationId || '')}"
          data-conversation-name="${this.escapeHtml(conversationName || 'Cuộc trò chuyện')}"
          data-conversation-avatar="${this.escapeHtml(conversationAvatar || '')}"
          data-conversation-token="${this.escapeHtml(match.partner_token || '')}">
          <img src="${this.escapeHtml(conversationAvatar || '/img/default-avatar.svg')}" alt="">
          <span class="all-message-search-result-text">
            <span class="all-message-search-result-heading">
              <strong>${this.escapeHtml(conversationName || 'Cuộc trò chuyện')}</strong>
              <time>${this.escapeHtml(this.formatTimeAgo(match.created_at))}</time>
            </span>
            <span class="all-message-search-result-snippet">${this.escapeHtml(sender)}: ${this.escapeHtml(text)}</span>
          </span>
        </button>`;
    };

    let rows = [];
    if (filter === 'all') {
      rows = [
        ...people.map(friend => renderContact(friend, false)),
        ...groups.map(group => renderContact(group, true)),
        ...messages.map(renderMessage)
      ];
    } else if (filter === 'people') {
      rows = people.map(friend => renderContact(friend, false));
    } else if (filter === 'messages') {
      rows = directMessages.map(renderMessage);
    } else if (filter === 'groups') {
      rows = [...groups.map(group => renderContact(group, true)), ...groupMessages.map(renderMessage)];
    }

    results.innerHTML = rows.length
      ? rows.join('')
      : '<div class="all-message-search-empty">Không tìm thấy kết quả phù hợp.</div>';
    results.style.display = 'block';
  }

  async openGlobalMessageResult(result) {
    const messageId = Number(result.dataset.messageId);
    const conversationId = result.dataset.conversationId;
    if (!conversationId) return;
    const dropdown = document.getElementById('allMessagesSearchResults');
    if (dropdown) dropdown.style.display = 'none';

    if (result.dataset.isGroup === 'true') {
      await this.openGroupChat(
        conversationId,
        result.dataset.conversationName || 'Nhóm chat',
        result.dataset.conversationAvatar || ''
      );
    } else {
      await this.openChatWith(
        conversationId,
        result.dataset.conversationName || 'Cuộc trò chuyện',
        result.dataset.conversationAvatar || '',
        result.dataset.conversationToken || conversationId
      );
    }
    if (result.dataset.resultKind === 'message' && messageId) {
      await this.jumpToChatMessage(messageId);
    }
  }

  removeVietnameseTonesClient(str) {
    if (!str) return '';
    return str.normalize('NFD')
      .replace(/[\u0300-\u036f]/g, '')
      .replace(/đ/g, 'd').replace(/Đ/g, 'D')
      .toLowerCase();
  }

  onChatSearchResultContextMenu(event, msgId) {
    if (event) {
      event.preventDefault();
      event.stopPropagation();
    }
    this.jumpToChatMessage(msgId, event);
  }

  async jumpToChatMessage(msgId, event) {
    if (event) {
      event.preventDefault();
      event.stopPropagation();
    }
    msgId = parseInt(msgId, 10);
    if (!this.currentChatPartner || !this.currentUser) return;
    if (!msgId) {
      this.showToast('Không tìm thấy tin nhắn gốc (tin nhắn cũ chưa lưu ID)', 'error');
      return;
    }

    // Ẩn dropdown tìm kiếm
    const dropdown = document.getElementById('chatSearchResultsDropdown');
    if (dropdown) dropdown.style.display = 'none';

    const listEl = document.getElementById('messagesList');
    const chatBody = document.getElementById('chatBody');
    if (!listEl || !chatBody) return;
    listEl.style.paddingTop = '';
    listEl.style.paddingBottom = '';

    // Nếu tin nhắn đã có trên DOM → cuộn căn giữa + highlight
    let targetEl = document.getElementById('msg-' + msgId);
    if (!targetEl) {
      // Tin nhắn chưa được tải → gọi API /around để nạp ngữ cảnh
      try {
        const isGroup = !!this.currentChatPartner.isGroup;
        const url = `/api/messages/around?my_id=${encodeURIComponent(this.currentUser.id)}&partner_id=${encodeURIComponent(this.currentChatPartner.id)}&target_id=${msgId}&is_group=${isGroup ? 'true' : 'false'}`;
        const res = await fetch(url);
        const data = await res.json();
        if (data.status === 'ok' && Array.isArray(data.messages) && data.messages.length > 0) {
          // Sắp xếp theo id tăng dần (cũ → mới)
          const msgs = data.messages.slice().sort((a, b) => (a.id || 0) - (b.id || 0));
          listEl.innerHTML = this.renderMessagesHtml(msgs);
          this.lastRenderedMsgSignature = msgs.map(m => `${m.id}_${m.is_revoked}_${m.reaction || ''}_${JSON.stringify(m.reactions || [])}_${JSON.stringify(m.receipts || [])}_${m.content}_${m.is_read || 0}`).join('|');
          if (msgs.length > 0) {
            this.oldestMessageId = msgs[0].id;
            this.hasMoreMessages = !isGroup;
          }
          const loadMoreBox = document.getElementById('loadMoreChatBox');
          if (loadMoreBox) loadMoreBox.style.display = (!isGroup && this.hasMoreMessages) ? 'block' : 'none';
          // Đang xem ngữ cảnh cũ: tạm dừng poll để không bị render đè, hiện nút quay về tin mới nhất
          this.isViewingHistory = true;
          this.bindChatScrollWatcher();
          this.setBackToLatestVisible(true);
          targetEl = document.getElementById('msg-' + msgId);
        }
      } catch (e) {
        console.error('jumpToChatMessage load around failed', e);
        this.showToast('Không tải được tin nhắn gốc', 'error');
        return;
      }
    }

    if (!targetEl) {
      this.showToast('Không tìm thấy tin nhắn trong đoạn chat', 'error');
      return;
    }

    // Căn giữa tin nhắn trong khung chat (căn lại sau khi ảnh/video tải xong làm đổi chiều cao)
    this.centerChatMessage(targetEl);
    requestAnimationFrame(() => this.centerChatMessage(targetEl));
    setTimeout(() => this.centerChatMessage(targetEl), 250);
    this.updateReadReceipt();

    // Làm nổi bật trong giây lát
    targetEl.classList.remove('msg-highlight-pulse');
    // Force reflow
    void targetEl.offsetWidth;
    targetEl.classList.add('msg-highlight-pulse');
    setTimeout(() => {
      targetEl.classList.remove('msg-highlight-pulse');
    }, 2600);

    this.chatJumpLayoutObserver?.disconnect();
    clearTimeout(this.chatJumpLayoutTimeout);
    if (typeof ResizeObserver === 'function') {
      this.chatJumpLayoutObserver = new ResizeObserver(() => {
        if (targetEl.classList.contains('msg-highlight-pulse')) this.centerChatMessage(targetEl);
      });
      this.chatJumpLayoutObserver.observe(targetEl);
      this.chatJumpLayoutTimeout = setTimeout(() => {
        this.chatJumpLayoutObserver?.disconnect();
        this.chatJumpLayoutObserver = null;
      }, 3000);
    }
  }

  centerChatMessage(targetEl) {
    const chatBody = document.getElementById('chatBody');
    if (!chatBody || !targetEl || !targetEl.isConnected) return;
    const listEl = document.getElementById('messagesList');
    if (listEl) {
      listEl.style.paddingTop = '';
      listEl.style.paddingBottom = '';
    }
    const bodyRect = chatBody.getBoundingClientRect();
    const elRect = targetEl.getBoundingClientRect();
    const offset = (elRect.top + elRect.height / 2) - (bodyRect.top + bodyRect.height / 2);
    const maxScroll = Math.max(0, chatBody.scrollHeight - chatBody.clientHeight);
    chatBody.scrollTop = Math.max(0, Math.min(chatBody.scrollTop + offset, maxScroll));
  }

  // Thoát chế độ xem ngữ cảnh cũ; reload=true thì tải lại tin nhắn mới nhất
  exitHistoryView(reload = true) {
    const wasViewing = this.isViewingHistory;
    this.isViewingHistory = false;
    this.setBackToLatestVisible(false);
    if (reload && wasViewing && this.currentChatPartner) {
      if (this.currentChatPartner.isGroup) this.loadGroupMessages(this.currentChatPartner.id);
      else this.loadInitialMessages();
    }
  }

  setBackToLatestVisible(show) {
    const chatBody = document.getElementById('chatBody');
    if (!chatBody) return;
    let btn = document.getElementById('chatBackToLatestBtn');
    if (!btn) {
      if (!show) return;
      btn = document.createElement('button');
      btn.id = 'chatBackToLatestBtn';
      btn.type = 'button';
      btn.className = 'chat-back-latest-btn';
      btn.textContent = '⬇ Tin nhắn mới nhất';
      btn.onclick = (e) => { e.stopPropagation(); this.exitHistoryView(true); };
      chatBody.appendChild(btn);
    }
    btn.style.display = show ? 'block' : 'none';
  }

  bindChatScrollWatcher() {
    if (this._chatScrollBound) return;
    const chatBody = document.getElementById('chatBody');
    if (!chatBody) return;
    this._chatScrollBound = true;
    chatBody.addEventListener('scroll', () => {
      if (!this.isViewingHistory) return;
      const nearBottom = chatBody.scrollHeight - chatBody.scrollTop - chatBody.clientHeight < 24;
      if (nearBottom) this.exitHistoryView(true);
    }, { passive: true });
  }

  // Rút gọn nội dung cho phần trích dẫn Reply: gộp khoảng trắng/xuống dòng, cắt bằng "..."
  makeReplySnippet(text, max) {
    const limit = max || this.REPLY_SNIPPET_MAX;
    const flat = String(text == null ? '' : text).replace(/\s+/g, ' ').trim();
    if (!flat) return '';
    const chars = Array.from(flat);
    return chars.length > limit ? chars.slice(0, limit).join('').trimEnd() + '...' : flat;
  }

  getReplySnippetForMessage(m) {
    if (!m) return '';
    if (m.is_revoked || m.content === 'Tin nhắn đã được thu hồi') return 'Tin nhắn đã được thu hồi';
    const content = (m.content || '').trim();
    if (content && !content.startsWith('__SYS_')) return this.makeReplySnippet(content);
    if (m.media_type === 'IMAGE') return '[Hình ảnh]';
    if (m.media_type === 'VIDEO') return '[Video]';
    if (m.shared_post_id) return '[Bài viết được chia sẻ]';
    return 'Đính kèm';
  }

  // --- ĐẶT BIỆT DANH CHO BẠN BÈ (Item 10) ---
  async chatActionSetNickname() {
    if (!this.currentChatPartner || this.currentChatPartner.isGroup) return;
    this.toggleChatMenu();
    document.getElementById('nicknameTargetName').textContent = this.currentChatPartner.name;
    const input = document.getElementById('nicknameInput');
    input.value = '';
    try {
      const res = await fetch(`/api/friends/nickname?user_id=${this.currentUser.id}&friend_id=${this.currentChatPartner.id}`);
      const data = await res.json();
      if (data.status === 'ok' && data.nickname) {
        input.value = data.nickname;
      }
    } catch (e) {}
    document.getElementById('nicknameModal').style.display = 'flex';
  }

  async submitNickname() {
    if (!this.currentChatPartner) return;
    const input = document.getElementById('nicknameInput');
    const nick = input.value.trim();
    try {
      const res = await fetch('/api/friends/nickname', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          user_id: this.currentUser.id,
          friend_id: this.currentChatPartner.id,
          nickname: nick
        })
      });
      const data = await res.json();
      if (data.status === 'ok') {
        document.getElementById('nicknameModal').style.display = 'none';
        this.showToast('Đã lưu biệt danh!', 'success');

        // Thông báo hệ thống trong đoạn tin nhắn (Hình 2)
        const systemMsgContent = nick
          ? `__SYS_NICK__:${this.currentUser.fullname}:${this.currentChatPartner.name}:${nick}`
          : `__SYS_NICK_REMOVE__:${this.currentUser.fullname}:${this.currentChatPartner.name}`;

        await fetch('/api/messages/send', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({
            sender_id: this.currentUser.id,
            recipient_id: this.currentChatPartner.id,
            content: systemMsgContent,
            media_type: 'SYSTEM'
          })
        });

        // Chỉ hiển thị biệt danh trên thanh chat, không hiển thị tên thật trong ngoặc đơn (Hình 3 & 4)
        document.getElementById('chatPartnerName').textContent = nick || this.currentChatPartner.name;
        this.loadInitialMessages();
        this.loadFriendsAndContacts();
      }
    } catch (e) {
      this.showToast('Lỗi: ' + e.message, 'error');
    }
  }



  // --- ĐỔI TÊN NHÓM TRÒ CHUYỆN ---
  chatActionRenameGroup() {
    if (!this.currentChatPartner || !this.currentChatPartner.isGroup) return;
    this.toggleChatMenu();

    if (!document.getElementById('renameGroupModal')) {
      const div = document.createElement('div');
      div.innerHTML = `
        <div class="modal-overlay" id="renameGroupModal" style="display:none; z-index: 2050;" onclick="if(event.target===this)this.style.display='none'">
          <div class="auth-card" style="max-width:400px; width: 92%;">
            <div class="auth-header" style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 1rem;">
              <h2 style="font-size: 1.2rem; font-weight: 700; margin: 0;">✏️ Đổi tên nhóm</h2>
              <button class="btn-icon" onclick="document.getElementById('renameGroupModal').style.display='none'" style="cursor: pointer;">✕</button>
            </div>
            <div class="form-group" style="margin-bottom: 1.2rem;">
              <label style="font-weight: 600; font-size: 0.85rem; margin-bottom: 0.35rem; display: block;">Tên nhóm mới:</label>
              <input type="text" id="renameGroupInput" placeholder="Nhập tên nhóm mới..." maxlength="60" style="width: 100%; padding: 0.6rem 0.8rem; border-radius: 8px; border: 1px solid var(--border-color); background: var(--bg-surface-secondary); color: var(--text-primary); font-size: 0.9rem;">
            </div>
            <div style="display:flex; gap:0.5rem;">
              <button class="btn-secondary" style="flex:1" onclick="document.getElementById('renameGroupModal').style.display='none'">Hủy</button>
              <button class="btn-primary" style="flex:1" onclick="app.submitRenameGroup()">Lưu tên nhóm</button>
            </div>
          </div>
        </div>
      `;
      document.body.appendChild(div.firstElementChild);
    }

    const inp = document.getElementById('renameGroupInput');
    if (inp) {
      inp.value = this.currentChatPartner.name || '';
      setTimeout(() => inp.focus(), 100);
    }
    const modal = document.getElementById('renameGroupModal');
    if (modal) modal.style.display = 'flex';
  }

  async submitRenameGroup() {
    if (!this.currentChatPartner || !this.currentChatPartner.isGroup) return;
    const inp = document.getElementById('renameGroupInput');
    const newName = (inp?.value || '').trim();
    if (!newName) {
      this.showToast('Vui lòng nhập tên nhóm!', 'error');
      return;
    }
    try {
      const res = await fetch('/api/groups/update-name', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          group_id: this.currentChatPartner.id,
          name: newName,
          user_id: this.currentUser ? this.currentUser.id : ''
        })
      });
      const data = await res.json();
      if (data.status === 'ok') {
        const modal = document.getElementById('renameGroupModal');
        if (modal) modal.style.display = 'none';
        this.showToast('Đổi tên nhóm thành công!', 'success');
        const oldName = this.currentChatPartner.name;
        this.currentChatPartner.name = newName;
        const chatTitle = document.getElementById('chatPartnerName');
        if (chatTitle) chatTitle.textContent = `👥 ${newName}`;
        // Gửi thông báo hệ thống đổi tên nhóm vào chat
        try {
          await fetch('/api/groups/messages/send', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
              group_id: this.currentChatPartner.id,
              sender_id: this.currentUser ? this.currentUser.id : '',
              content: `__SYS_RENAME__:${oldName}:${newName}`,
              shared_post_id: '',
              media_type: 'SYSTEM'
            })
          });
          // Reload lại tin nhắn để hiển thị thông báo hệ thống
          this.loadGroupMessages(this.currentChatPartner.id);
        } catch (_) {}
        this.loadFriendsAndContacts();
      } else {
        this.showToast(data.message || 'Lỗi cập nhật tên nhóm!', 'error');
      }
    } catch (e) {
      this.showToast('Lỗi kết nối: ' + e.message, 'error');
    }
  }

  // --- QUẢN LÝ THÀNH VIÊN NHÓM (Item 13) ---
  async chatActionViewMembers() {
    if (!this.currentChatPartner || !this.currentChatPartner.isGroup) return;
    this.toggleChatMenu();
    try {
      const res = await fetch(`/api/groups/members?group_id=${encodeURIComponent(this.currentChatPartner.id)}`);
      const data = await res.json();
      if (data.status === 'ok') {
        const listEl = document.getElementById('groupMembersList');
        const isAdmin = (this.currentChatPartner.role === 'ADMIN');
        listEl.innerHTML = data.members.map(m => {
          const isMe = (m.id === this.currentUser.id);
          const roleLabel = m.role === 'ADMIN' ? '👑 Trưởng nhóm' : 'Thành viên';
          let actionBtn = '';
          if (isAdmin && !isMe && m.role !== 'ADMIN') {
            actionBtn = `<button class="btn-secondary" style="color: var(--accent-red); font-size: 0.75rem; padding: 2px 8px;" onclick="app.removeMemberFromGroup(${app.jsArg(m.id)})">Xóa</button>`;
          }
          return `
            <div style="display: flex; align-items: center; justify-content: space-between; padding: 0.5rem 0; border-bottom: 1px solid var(--border-color);">
              <div style="display: flex; align-items: center; gap: 0.6rem; cursor: pointer;" onclick="app.openUserProfile(${app.jsArg(m.user_token || m.id)})">
                <img src="${app.escapeHtml(m.avatar || '/img/default-avatar.svg')}" style="width: 38px; height: 38px; border-radius: 50%; object-fit: cover;">
                <div>
                  <div style="font-weight: 600; font-size: 0.88rem;">${this.escapeHtml(m.name)} ${isMe ? '(Bạn)' : ''}</div>
                  <div style="font-size: 0.75rem; color: var(--text-muted);">${roleLabel}</div>
                </div>
              </div>
              ${actionBtn}
            </div>
          `;
        }).join('');
        document.getElementById('groupMembersModal').style.display = 'flex';
      }
    } catch (e) {}
  }

  async chatActionAddMember() {
    if (!this.currentChatPartner || !this.currentChatPartner.isGroup) return;
    this.toggleChatMenu();
    try {
      const res = await fetch(`/api/groups/friends-to-add?user_id=${this.currentUser.id}&group_id=${this.currentChatPartner.id}`);
      const data = await res.json();
      if (!res.ok || data.status !== 'ok') {
        this.showToast(data.message || 'Không thể tải danh sách bạn bè để mời.', 'error');
        return;
      }
      const listEl = document.getElementById('addGroupMemberList');
      if (!listEl) return;
      const inviteButton = document.getElementById('inviteSelectedGroupMembersBtn');
      const friends = Array.isArray(data.friends) ? data.friends : [];
      if (inviteButton) inviteButton.disabled = friends.length === 0;
      if (friends.length === 0) {
        listEl.innerHTML = '<div style="text-align: center; color: var(--text-muted); padding: 1.5rem; font-size: 0.85rem;">Tất cả bạn bè của bạn đã có mặt trong nhóm!</div>';
      } else {
        listEl.innerHTML = friends.map(f => `
            <div style="display: flex; align-items: center; justify-content: space-between; padding: 0.5rem 0; border-bottom: 1px solid var(--border-color);">
              <div style="display: flex; align-items: center; gap: 0.6rem;">
                <img src="${app.escapeHtml(f.avatar || '/img/default-avatar.svg')}" style="width: 38px; height: 38px; border-radius: 50%; object-fit: cover;">
                <div style="font-weight: 600; font-size: 0.88rem;">${this.escapeHtml(f.name)}</div>
              </div>
              <label class="group-member-select">
                <input type="checkbox" class="group-member-invite-checkbox" value="${this.escapeHtml(f.id)}">
                <span>Mời</span>
              </label>
            </div>
          `).join('');
      }
      document.getElementById('addGroupMemberModal').style.display = 'flex';
    } catch (e) {
      this.showToast(`Không thể tải danh sách bạn bè: ${e.message}`, 'error');
    }
  }

  async submitSelectedGroupMembers() {
    if (!this.currentChatPartner) return;
    const userIds = [...document.querySelectorAll('.group-member-invite-checkbox:checked')].map(input => input.value);
    if (!userIds.length) {
      this.showToast('Vui lòng chọn ít nhất một thành viên.', 'error');
      return;
    }
    try {
      const res = await fetch('/api/groups/members/add', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          group_id: this.currentChatPartner.id,
          user_ids: userIds,
          requester_id: this.currentUser.id
        })
      });
      const data = await res.json();
      if (res.ok && data.status === 'ok') {
        this.showToast(data.message || 'Đã mời thành viên vào nhóm!', 'success');
        document.getElementById('addGroupMemberModal').style.display = 'none';
        await this.loadGroupMessages(this.currentChatPartner.id);
        await this.loadFriendsAndContacts();
      } else {
        this.showToast(data.message || 'Lỗi mời thành viên', 'error');
      }
    } catch (e) {
      this.showToast(`Không thể mời thành viên: ${e.message}`, 'error');
    }
  }

  chatActionRemoveMember() {
    this.chatActionViewMembers();
  }

  async removeMemberFromGroup(userId) {
    const _c3 = await this.showConfirm('Xóa thành viên này khỏi nhóm?'); if (!_c3) return;
    try {
      const res = await fetch('/api/groups/members/remove', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          group_id: this.currentChatPartner.id,
          user_id: userId,
          requester_id: this.currentUser.id
        })
      });
      const data = await res.json();
      if (data.status === 'ok') {
        this.showToast('Đã xóa thành viên khỏi nhóm!', 'success');
        this.chatActionViewMembers();
      }
    } catch (e) {}
  }

  async chatActionLeaveGroup() {
    const _c4 = await this.showConfirm('Rời khỏi nhóm trò chuyện này?'); if (!_c4) return;
    try {
      const res = await fetch('/api/groups/leave', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          group_id: this.currentChatPartner.id,
          user_id: this.currentUser.id
        })
      });
      const data = await res.json();
      if (data.status === 'ok') {
        this.showToast('Đã rời khỏi nhóm!', 'success');
        this.closeChat();
        this.loadFriendsAndContacts();
      }
    } catch (e) {}
  }

  async chatActionClearGroupMessages() {
    const _c5 = await this.showConfirm('Xóa toàn bộ lịch sử tin nhắn nhóm cho bạn?'); if (!_c5) return;
    const msgRows = document.querySelectorAll('#messagesList .message-row');
    msgRows.forEach(async r => {
      const idStr = r.id.replace('msg-', '');
      if (idStr) {
        await fetch('/api/groups/messages/delete', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ message_id: parseInt(idStr), user_id: this.currentUser.id })
        });
      }
    });
    document.getElementById('messagesList').innerHTML = '<div style="text-align: center; font-size: 0.8rem; color: var(--text-muted); padding: 1.5rem;">Đoạn trò chuyện đã được làm sạch.</div>';
    this.toggleChatMenu();
  }

  async loadInitialMessages() {
    if (!this.currentChatPartner || this.currentChatPartner.isGroup) return;
    this.exitHistoryView(false);
    try {
      const res = await fetch(`/api/messages?my_id=${this.currentUser.id}&partner_id=${this.currentChatPartner.id}&limit=30&before_id=0`);
      const data = await res.json();
      if (data.status === 'ok') {
        const listEl = document.getElementById('messagesList');
        const loadMoreBox = document.getElementById('loadMoreChatBox');
        this.hasMoreMessages = data.has_more;

        if (data.messages.length === 0) {
          listEl.innerHTML = '<div style="text-align: center; font-size: 0.8rem; color: var(--text-muted); padding: 1.5rem;">Chưa có tin nhắn nào. Hãy gửi lời chào đầu tiên! 👋</div>';
          if (loadMoreBox) loadMoreBox.style.display = 'none';
          return;
        }

        this.oldestMessageId = data.messages[0].id;
        this.lastRenderedMsgSignature = data.messages.map(m => `${m.id}_${m.is_revoked}_${m.reaction || ''}_${JSON.stringify(m.reactions || [])}_${JSON.stringify(m.receipts || [])}_${m.content}_${m.is_read || 0}`).join('|');
        listEl.innerHTML = this.renderMessagesHtml(data.messages);

        if (loadMoreBox) {
          loadMoreBox.style.display = this.hasMoreMessages ? 'block' : 'none';
        }

        const chatBody = document.getElementById('chatBody');
        if (chatBody) chatBody.scrollTop = chatBody.scrollHeight;
        this.loadFriendsAndContacts();
        this.updateReadReceipt();
        // Bắt đầu poll trạng thái đã đọc
        this.startReadReceiptPoll();
      }
    } catch (e) {
      console.error(e);
    }
  }

  renderMessageContent(m) {
    if (m.is_revoked || m.content === 'Tin nhắn đã được thu hồi') {
      return `<div class="message-text msg-recalled-text"><em>Tin nhắn đã được thu hồi</em></div>`;
    }

    let html = '';
    if (m.reply_to_text || m.reply_to_id) {
      const origId = parseInt(m.reply_to_id, 10) || 0;
      // Chỉ hiện đoạn ngắn (cắt bằng "..."), CSS giới hạn thêm 1 dòng
      const quoteText = this.makeReplySnippet(m.reply_to_text) || this.getReplySnippetForMessage(this.msgCache[origId]) || 'Tin nhắn gốc';
      html += `<div class="chat-reply-quote" data-reply-id="${origId}" onclick="app.jumpToChatMessage(${origId}, event)" title="Nhấn để chuyển tới tin nhắn gốc">↪️ ${this.escapeHtml(quoteText)}</div>`;
    }
    const emojiSize = String(m.media_type || '').match(/^EMOJI_([123])$/)?.[1];
    if (emojiSize && m.content) {
      html += `<span class="message-emoji size-${emojiSize}" aria-label="Biểu tượng cảm xúc">${this.escapeHtml(m.content)}</span>`;
    } else if (m.content) {
      html += `<div class="message-text">${this.escapeHtml(m.content)}</div>`;
    }

    // Media đính kèm (Hình ảnh & Video) (Item 1)
    if (m.media_url) {
      if (m.media_type === 'IMAGE') {
        html += `<div class="chat-media-wrapper media-viewer-trigger" data-media-viewer="IMAGE" data-media-url="${this.escapeHtml(m.media_url)}"><img src="${this.escapeHtml(m.media_url)}" class="chat-media-img" alt="Ảnh"></div>`;
      } else if (m.media_type === 'VIDEO') {
        html += `<div class="chat-media-wrapper media-viewer-trigger" data-media-viewer="VIDEO" data-media-url="${this.escapeHtml(m.media_url)}"><video src="${this.escapeHtml(m.media_url)}" controls class="chat-media-video"></video></div>`;
      }
    }

    if (m.shared_post_id) {
      const uniqueCardId = `shared-post-${m.id || Math.floor(Math.random()*1000000)}`;
      html += `
        <div class="shared-post-bubble-card" id="${uniqueCardId}" onclick="app.viewSharedPost(${app.jsArg(m.shared_post_id)})">
          <div style="font-size: 0.75rem; color: var(--text-muted); display: flex; align-items: center; gap: 4px;">
            <span>🔄 Đang tải bài viết được chia sẻ...</span>
          </div>
        </div>
      `;
      setTimeout(() => this.loadSharedPostPreview(uniqueCardId, m.shared_post_id), 20);
    }
    return html;
  }

  async loadSharedPostPreview(cardId, postId) {
    const el = document.getElementById(cardId);
    if (!el) return;
    try {
      const res = await fetch(`/api/posts/detail?id=${encodeURIComponent(postId)}&current_user_id=${this.currentUser ? this.currentUser.id : ''}`);
      const data = await res.json();
      if (data.status === 'ok' && data.post) {
        const p = data.post;
        let mediaHtml = '';
        if (p.media_url) {
          mediaHtml = this.renderPostMedia(p.media_type, p.media_url, 'shared-post-bubble-media');
        }
        el.innerHTML = `
          <div class="shared-post-bubble-header">
            <img src="${app.escapeHtml(p.author_avatar || '/img/default-avatar.svg')}" alt="Avatar">
            <div class="shared-post-bubble-author">${this.escapeHtml(p.author_name)}</div>
          </div>
          ${p.title ? `<div class="shared-post-bubble-title">${this.escapeHtml(p.title)}</div>` : ''}
          <div class="shared-post-bubble-content">${this.escapeHtml(p.content || '')}</div>
          ${mediaHtml}
          <div style="font-size: 0.7rem; color: var(--text-muted); margin-top: 0.35rem;">
            <span>Nhấn để xem chi tiết bài viết ↗</span>
          </div>
        `;
      } else {
        el.innerHTML = `<div style="font-size: 0.75rem; color: var(--text-muted); font-style: italic;">Bài viết đã bị xóa hoặc không khả dụng.</div>`;
      }
    } catch (e) {
      el.innerHTML = `<div style="font-size: 0.75rem; color: var(--text-muted); font-style: italic;">Không thể tải bài viết.</div>`;
    }
  }

  viewSharedPost(postId) {
    if (this.openCommentModal) {
      this.openCommentModal(postId);
    }
  }

  async loadGroupMessages(groupId) {
    this.exitHistoryView(false);
    try {
      const res = await fetch(`/api/groups/messages?group_id=${encodeURIComponent(groupId)}&viewer_id=${this.currentUser ? this.currentUser.id : ''}&limit=30`);
      const data = await res.json();
      if (data.status === 'ok') {
        const listEl = document.getElementById('messagesList');
        const loadMoreBox = document.getElementById('loadMoreChatBox');
        if (loadMoreBox) loadMoreBox.style.display = 'none';

        if (data.messages.length === 0) {
          listEl.innerHTML = '<div style="text-align: center; font-size: 0.8rem; color: var(--text-muted); padding: 1.5rem;">Chưa có tin nhắn nào trong nhóm. Hãy gửi tin nhắn đầu tiên!</div>';
          return;
        }

        this.lastRenderedMsgSignature = data.messages.map(m => `${m.id}_${m.is_revoked}_${m.reaction || ''}_${JSON.stringify(m.reactions || [])}_${JSON.stringify(m.receipts || [])}_${m.content}_${m.is_read || 0}`).join('|');
        listEl.innerHTML = this.renderMessagesHtml(data.messages);

        const chatBody = document.getElementById('chatBody');
        if (chatBody) chatBody.scrollTop = chatBody.scrollHeight;
        // Show read receipt after render
        this.updateReadReceipt();
      }
    } catch (e) {
      console.error(e);
    }
  }

  async loadOlderMessages() {
    if (!this.currentChatPartner || this.currentChatPartner.isGroup || !this.hasMoreMessages || this.isLoadingOlderMessages) return;
    this.isLoadingOlderMessages = true;
    const chatBody = document.getElementById('chatBody');
    const listEl = document.getElementById('messagesList');
    const prevScrollHeight = chatBody.scrollHeight;

    try {
      const res = await fetch(`/api/messages?my_id=${this.currentUser.id}&partner_id=${this.currentChatPartner.id}&limit=30&before_id=${this.oldestMessageId}`);
      const data = await res.json();
      if (data.status === 'ok' && data.messages.length > 0) {
        this.hasMoreMessages = data.has_more;
        this.oldestMessageId = data.messages[0].id;
        const olderHtml = this.renderMessagesHtml(data.messages);
        listEl.insertAdjacentHTML('afterbegin', olderHtml);
        chatBody.scrollTop = chatBody.scrollHeight - prevScrollHeight;
        this.updateReadReceipt();
      } else {
        this.hasMoreMessages = false;
      }
    } catch (e) {
      console.error(e);
    } finally {
      this.isLoadingOlderMessages = false;
    }
  }

  renderMessagesHtml(messages) {
    if (!Array.isArray(messages) || !messages.length) return '';
    const latestReadMap = {};
    for (const m of messages) {
      if (!Array.isArray(m.receipts)) continue;
      for (const r of m.receipts) {
        if (r.user_id && r.user_id !== m.sender_id && r.read_at) {
          latestReadMap[r.user_id] = Math.max(latestReadMap[r.user_id] || 0, Number(m.id) || 0);
        }
      }
    }
    let latestSentByMeId = 0;
    for (let i = messages.length - 1; i >= 0; i--) {
      if (messages[i].sender_id === this.currentUser?.id) {
        latestSentByMeId = Number(messages[i].id) || 0;
        break;
      }
    }
    return messages.map((m, i) => {
      const isLatestSent = Number(m.id) === latestSentByMeId;
      return this.renderMessageItem(m, messages[i - 1] || null, messages[i + 1] || null, latestReadMap, isLatestSent);
    }).join('');
  }

  renderMessageReceiptStatus(message, isMe, isGroup, latestReadMap = {}, isLatestSentByMe = false) {
    const receipts = Array.isArray(message.receipts) ? message.receipts : [];
    const recipients = receipts.filter(receipt => receipt.user_id && receipt.user_id !== message.sender_id);
    const allReadersOfThisMsg = recipients.filter(receipt => receipt.read_at);
    const activeReadersAtThisMsg = recipients.filter(receipt =>
      receipt.read_at && latestReadMap[receipt.user_id] === message.id
    );
    const deliveredCount = recipients.filter(receipt => receipt.delivered_at).length;
    const openDetails = `app.openMessageReceiptsModal(${Number(message.id)}, event)`;

    // 1. Nhắn tin nhóm (Group Chat)
    if (isGroup) {
      if (activeReadersAtThisMsg.length > 0) {
        const displayAvatars = activeReadersAtThisMsg.slice(0, 3);
        const overflowCount = activeReadersAtThisMsg.length - 3;
        const avatarList = displayAvatars.map(receipt => {
          const time = receipt.read_at ? this.formatChatMessageTime(receipt.read_at) : '';
          const tooltip = time ? `${receipt.name || 'Người xem'} · ${time}` : (receipt.name || 'Người xem');
          return `<img src="${this.escapeHtml(receipt.avatar || '/img/default-avatar.svg')}" 
                       alt="${this.escapeHtml(receipt.name || 'Người xem')}" 
                       title="${this.escapeHtml(tooltip)}">`;
        }).join('');
        const extraCount = overflowCount > 0
          ? `<span class="message-reader-overflow" title="Và ${overflowCount} người khác">+${overflowCount}</span>`
          : '';
        return `
          <div class="message-receipt-status group-message-receipts">
            <button type="button" class="message-reader-avatars" onclick="${openDetails}" aria-label="Xem ${activeReadersAtThisMsg.length} người đã xem">
              ${avatarList}${extraCount}
            </button>
          </div>
        `;
      }
      if (isMe && isLatestSentByMe) {
        const label = deliveredCount > 0 ? 'Đã nhận' : 'Đã gửi';
        return `
          <div class="message-receipt-status">
            <span class="message-delivery-label" onclick="${openDetails}" title="Xem chi tiết">${label}</span>
          </div>
        `;
      }
      return '';
    }

    // 2. Nhắn tin 1-1
    if (!isMe) return '';

    const partnerId = this.currentChatPartner ? this.currentChatPartner.id : '';
    const partnerRead = allReadersOfThisMsg.find(r => r.user_id === partnerId);
    const isLatestReadByPartner = partnerRead && (latestReadMap[partnerId] === message.id);

    if (isLatestReadByPartner) {
      const time = partnerRead.read_at ? this.formatChatMessageTime(partnerRead.read_at) : '';
      const tooltip = time ? `${partnerRead.name || 'Người nhận'} · Đã xem ${time}` : (partnerRead.name || 'Người nhận');
      return `
        <div class="message-receipt-status direct-message-receipt">
          <button type="button" class="message-reader-avatars" onclick="${openDetails}" aria-label="Xem chi tiết người đã xem">
            <img src="${this.escapeHtml(partnerRead.avatar || '/img/default-avatar.svg')}" 
                 alt="${this.escapeHtml(partnerRead.name || 'Người nhận')}" 
                 title="${this.escapeHtml(tooltip)}">
          </button>
        </div>
      `;
    }

    if (isLatestSentByMe) {
      const label = deliveredCount > 0 ? 'Đã nhận' : 'Đã gửi';
      return `
        <div class="message-receipt-status">
          <span class="message-delivery-label" onclick="${openDetails}" title="Xem chi tiết">${label}</span>
        </div>
      `;
    }

    return '';
  }

  renderMessageItem(m, prevMsg = null, nextMsg = null, latestReadMap = {}, isLatestSentByMe = false) {
    if (m && m.id) this.msgCache[m.id] = m;
    const isMe = (m.sender_id === this.currentUser.id);
    const isGroup = this.currentChatPartner && this.currentChatPartner.isGroup;
    const isRecalled = m.is_revoked || m.content === 'Tin nhắn đã được thu hồi';

    // Xử lý thông báo hệ thống đặt/gỡ biệt danh (Hình 2)
    let isSystemNickMsg = false;
    let systemNickText = '';

    if (m.content && m.content.startsWith('__SYS_RENAME__:')) {
      isSystemNickMsg = true;
      const parts = (m.content || '').split(':');
      const newName = parts.slice(2).join(':');
      if (isMe) {
        systemNickText = `Bạn đã đổi tên nhóm thành "${newName}".`;
      } else {
        const senderName = m.sender_name || 'Thành viên';
        systemNickText = `${senderName} đã đổi tên nhóm thành "${newName}".`;
      }
    } else if (m.content && m.content.startsWith('__SYS_NICK__:')) {
      isSystemNickMsg = true;
      const parts = (m.content || '').split(':');
      const setter = parts[1] || (isMe ? 'Bạn' : (m.sender_name || 'Đối phương'));
      const target = parts[2] || (this.currentChatPartner ? this.currentChatPartner.name : 'bạn bè');
      const nick = parts.slice(3).join(':');
      if (isMe) {
        systemNickText = `Bạn đã đặt biệt danh cho ${target} thành ${nick}.`;
      } else {
        systemNickText = `${setter} đã đặt biệt danh cho bạn là ${nick}.`;
      }
    } else if (m.content && m.content.startsWith('__SYS_NICK_REMOVE__:')) {
      isSystemNickMsg = true;
      const parts = (m.content || '').split(':');
      const setter = parts[1] || (isMe ? 'Bạn' : (m.sender_name || 'Đối phương'));
      const target = parts[2] || (this.currentChatPartner ? this.currentChatPartner.name : 'bạn bè');
      if (isMe) {
        systemNickText = `Bạn đã gỡ biệt danh của ${target}.`;
      } else {
        systemNickText = `${setter} đã gỡ biệt danh của bạn.`;
      }
    } else if (m.content && m.content.startsWith('__SYS_GROUP_INVITE__:')) {
      isSystemNickMsg = true;
      try {
        const invitees = JSON.parse(m.content.slice('__SYS_GROUP_INVITE__:'.length));
        const names = Array.isArray(invitees) ? invitees.filter(name => typeof name === 'string') : [];
        const inviter = isMe ? 'Bạn' : (m.sender_name || 'Thành viên');
        systemNickText = names.length
          ? `${inviter} đã mời ${names.join(', ')} vào nhóm.`
          : `${inviter} đã mời thành viên vào nhóm.`;
      } catch (error) {
        console.error('Không thể phân tích thông báo mời thành viên:', error);
        systemNickText = 'Thông báo mời thành viên vào nhóm.';
      }
    } else if (m.content && m.content.startsWith('__SYS_QUICK_EMOJI__:')) {
      isSystemNickMsg = true;
      const emoji = m.content.slice('__SYS_QUICK_EMOJI__:'.length);
      systemNickText = `${isMe ? 'Bạn' : (m.sender_name || 'Thành viên')} đã thay đổi biểu tượng cảm xúc thành ${emoji}`;
    } else if (m.media_type === 'SYSTEM') {
      isSystemNickMsg = true;
      systemNickText = m.content || 'Thông báo hệ thống';
    } else if (m.content && typeof m.content === 'string') {
      const matchNick = m.content.match(/^(.+?)\s+đã đặt biệt danh cho\s+(.+?)\s+(?:là|thành)\s+(.+)$/);
      if (matchNick) {
        isSystemNickMsg = true;
        const setter = matchNick[1];
        const target = matchNick[2];
        const nick = matchNick[3];
        if (isMe) {
          systemNickText = `Bạn đã đặt biệt danh cho ${target} thành ${nick}`;
        } else {
          systemNickText = `${setter} đã đặt biệt danh cho bạn là ${nick}`;
        }
      } else {
        const matchRemove = m.content.match(/^(.+?)\s+đã gỡ biệt danh của\s+(.+)$/);
        if (matchRemove) {
          isSystemNickMsg = true;
          const setter = matchRemove[1];
          const target = matchRemove[2];
          if (isMe) {
            systemNickText = `Bạn đã gỡ biệt danh của ${target}`;
          } else {
            systemNickText = `${setter} đã gỡ biệt danh của bạn`;
          }
        }
      }
    }

    // Nếu là nhóm chat mà thông báo chứa "đặt biệt danh" (do phiên bản cũ lưu nhầm), chuyển thành đổi tên nhóm
    if (isGroup && isSystemNickMsg && systemNickText.includes('đặt biệt danh')) {
      const currentGrpName = (this.currentChatPartner && this.currentChatPartner.name) || '';
      systemNickText = isMe
        ? `Bạn đã đổi tên nhóm thành "${currentGrpName}".`
        : `Thành viên đã đổi tên nhóm thành "${currentGrpName}".`;
    }

    if (isSystemNickMsg) {
      return `
        <div class="chat-system-message-row" id="msg-${m.id || ''}">
          <div class="chat-system-message-time">${m.created_at ? this.escapeHtml(this.formatChatMessageTime(m.created_at)) : ''}</div>
          <div class="chat-system-message">${this.escapeHtml(systemNickText)}</div>
        </div>
      `;
    }

    // Nhóm tin nhắn theo người gửi và thời gian (Hình 1)
    const isSameSenderAsPrev = prevMsg && (prevMsg.sender_id === m.sender_id) && prevMsg.created_at && m.created_at && 
      (Math.abs(new Date(m.created_at.replace(' ', 'T')) - new Date(prevMsg.created_at.replace(' ', 'T'))) < 5 * 60 * 1000);
    const isSameSenderAsNext = nextMsg && (nextMsg.sender_id === m.sender_id) && nextMsg.created_at && m.created_at && 
      (Math.abs(new Date(nextMsg.created_at.replace(' ', 'T')) - new Date(m.created_at.replace(' ', 'T'))) < 5 * 60 * 1000);

    const isSingle = !isSameSenderAsPrev && !isSameSenderAsNext;
    const isFirst = !isSameSenderAsPrev && isSameSenderAsNext;
    const isMiddle = isSameSenderAsPrev && isSameSenderAsNext;
    const isLast = isSameSenderAsPrev && !isSameSenderAsNext;

    const clusterClass = isSingle ? 'msg-cluster-single' : (isFirst ? 'msg-cluster-first msg-clustered' : (isMiddle ? 'msg-cluster-middle msg-clustered' : 'msg-cluster-last'));

    // Tên người gửi ở đầu cụm (Hình 1: như chữ "P")
    let senderHeaderHtml = '';
    if (!isMe && (isFirst || isSingle)) {
      const displayName = m.sender_name || (this.currentChatPartner ? this.currentChatPartner.name : 'Bạn bè');
      senderHeaderHtml = `<div class="chat-sender-header">${this.escapeHtml(displayName)}</div>`;
    }

    // Avatar: chỉ hiển thị ở tin nhắn cuối cùng của cụm (như Hình 1), các tin trước dùng spacer để thẳng hàng
    let avatarHtml = '';
    if (!isMe) {
      if (isLast || isSingle) {
        if (isGroup) {
          const sid = this.escapeHtml(m.sender_id || '');
          const sname = this.escapeHtml(m.sender_name || 'Người dùng');
          const savatar = this.escapeHtml(m.sender_avatar || '/img/default-avatar.svg');
          avatarHtml = `<img class="chat-avatar-mini chat-avatar-clickable" src="${app.escapeHtml(m.sender_avatar || '/img/default-avatar.svg')}" alt="Avatar"
            style="cursor:pointer;" title="Xem tùy chọn"
            onclick="event.stopPropagation(); app.openGroupMemberAvatarMenu(event, ${app.jsArg(sid)}, ${app.jsArg(sname)}, ${app.jsArg(savatar)})">`;
        } else {
          avatarHtml = `<img class="chat-avatar-mini" src="${app.escapeHtml(m.sender_avatar || '/img/default-avatar.svg')}" alt="Avatar"
            style="cursor:pointer;" title="Xem trang cá nhân"
            onclick="event.stopPropagation(); app.openUserProfile(${app.jsArg(m.sender_id || '')})">`;
        }
      } else {
        avatarHtml = `<div class="chat-avatar-spacer"></div>`;
      }
    }

    // Thanh hành động khi lia chuột (Hình 2)
    let actionsHtml = '';
    if (isRecalled) {
      actionsHtml = `
        <div class="msg-action-bar-circle recalled-message-actions">
          <button class="msg-circle-btn" onclick="app.deleteMessage(${m.id})" title="Xóa tin nhắn đã thu hồi">🗑️</button>
        </div>
      `;
    } else {
      const moreBtn = `
        <div style="position: relative;">
          <button class="msg-circle-btn" onclick="app.toggleMessageMoreMenu(${m.id}, event)" title="Thêm">⋮</button>
          <div class="msg-more-dropdown" id="msg-more-${m.id}">
            <button class="msg-dropdown-item" onclick="app.openMessageReceiptsModal(${m.id}, event)">ℹ️ Thông tin tin nhắn</button>
            ${isMe ? `
              <button class="msg-dropdown-item" onclick="app.handleRecallClick(${m.id}, '${m.created_at || ''}')">↩️ Thu hồi</button>
              <button class="msg-dropdown-item danger" onclick="app.deleteMessage(${m.id})">🗑️ Xóa</button>
            ` : `
              <button class="msg-dropdown-item danger" onclick="app.deleteMessage(${m.id})">🗑️ Xóa</button>
              <button class="msg-dropdown-item" onclick="app.reportMessage(${m.id}, ${app.jsArg(m.sender_id)})">🚩 Báo cáo</button>
            `}
          </div>
        </div>
      `;

      const replyBtn = `
        <button class="msg-circle-btn" onclick="app.startReplyToMessage(${m.id})" title="Trả lời">
          ↩️
        </button>
      `;

      const reactBtn = `
        <div id="msg-react-wrap-${m.id}" style="position: relative;">
          <button class="msg-circle-btn" onclick="app.toggleMessageReactionPicker(${m.id}, event)" title="Thả icon">☺️</button>
          <div class="msg-reaction-picker" id="msg-react-picker-${m.id}" data-origin-wrapper-id="msg-react-wrap-${m.id}" data-mode="message" data-target-id="${m.id}"></div>
        </div>
      `;

      // Căn chỉnh thứ tự 3 nút:
      // Tin nhắn bản thân: bên trái bubble -> [ ⋮ Thêm ] [ ↩️ Trả lời ] [ ☺️ Thả icon ]
      // Tin nhắn bạn bè: bên phải bubble -> [ ☺️ Thả icon ] [ ↩️ Trả lời ] [ ⋮ Thêm ]
      if (isMe) {
        actionsHtml = `
          <div class="msg-action-bar-circle">
            ${moreBtn}
            ${replyBtn}
            ${reactBtn}
          </div>
        `;
      } else {
        actionsHtml = `
          <div class="msg-action-bar-circle">
            ${reactBtn}
            ${replyBtn}
            ${moreBtn}
          </div>
        `;
      }
    }

    // Mốc thời gian nằm trên tin đầu tiên sau khoảng nghỉ ít nhất 3 phút.
    const previousTime = prevMsg?.created_at ? new Date(prevMsg.created_at.replace(' ', 'T')).getTime() : NaN;
    const currentTime = m.created_at ? new Date(m.created_at.replace(' ', 'T')).getTime() : NaN;
    const showTime = !Number.isFinite(previousTime) || !Number.isFinite(currentTime) || currentTime - previousTime >= 3 * 60 * 1000;
    const timeStr = m.created_at ? this.formatChatMessageTime(m.created_at) : '';
    const timeHtml = showTime ? `<div class="message-time" title="${this.escapeHtml(this.formatChatMessageTooltip(m.created_at))}">${this.escapeHtml(timeStr)}</div>` : '';
    const receiptHtml = this.renderMessageReceiptStatus(m, isMe, !!isGroup, latestReadMap, isLatestSentByMe);

    // Reaction badge ở góc dưới bubble — ánh xạ LIKE/LOVE sang icon chuẩn
    const legacyReactionIcons = { LIKE: '👍', LOVE: '💗', HAHA: '😆', WOW: '😮', SAD: '😢', ANGRY: '😡' };
    const reactions = Array.isArray(m.reactions) && m.reactions.length
      ? m.reactions
      : (m.reaction ? [{ emoji: legacyReactionIcons[m.reaction] || m.reaction, count: 1, users: '' }] : []);
    const sortedReactions = [...reactions].sort((a, b) => (Number(b.count) || 0) - (Number(a.count) || 0));
    const reactionCount = Number(m.reaction_count) || sortedReactions.reduce((total, item) => total + (Number(item.count) || 0), 0);
    const reactionIcons = sortedReactions.slice(0, 3).map(item => item.emoji || '').filter(Boolean);
    const reactionTitle = sortedReactions.map(item => {
      const users = Array.isArray(item.users) ? item.users.map(user => user.name).join(', ') : item.users;
      return `${item.emoji || ''} ${item.count || 0}${users ? `: ${users}` : ''}`;
    }).join('\n');
    const reactionBadgeHtml = reactionCount
      ? `<button type="button" class="msg-reaction-badge" title="${this.escapeHtml(reactionTitle)}" aria-label="Xem ${reactionCount} lượt cảm xúc" onclick="app.openMessageReactionsModal(${m.id}, event)">${reactionIcons.map(icon => `<span>${this.escapeHtml(icon)}</span>`).join('')}<span class="msg-reaction-count">${reactionCount}</span></button>`
      : '';

    const emojiSize = String(m.media_type || '').match(/^EMOJI_([123])$/)?.[1];
    const emojiOnly = !!emojiSize && !!m.content && !m.media_url && !m.shared_post_id && !m.reply_to_text && !m.reply_to_id;
    const tooltipTime = m.created_at ? this.formatChatMessageTooltip(m.created_at) : '';
    const bubbleHtml = `
      <div class="message-bubble${emojiOnly ? ' emoji-only' : ''}"
           onmouseenter="app.startMessageTimestampTimer(${m.id})"
           onmouseleave="app.cancelMessageTimestampTimer(${m.id})">
        ${this.renderMessageContent(m)}
        ${reactionBadgeHtml}
      </div>
    `;

    return `${timeHtml}${senderHeaderHtml}
      <div class="message-row ${isMe ? 'me' : 'other'} ${clusterClass}" id="msg-${m.id || ''}">
        <div class="message-main">
          ${avatarHtml}
          <div class="message-content-wrapper">
            <div class="message-bubble-container">
              <div class="message-hover-timestamp">${this.escapeHtml(tooltipTime)}</div>
              ${isMe ? `${actionsHtml}${bubbleHtml}` : `${bubbleHtml}${actionsHtml}`}
            </div>
          </div>
        </div>
        ${receiptHtml}
      </div>
    `;
  }

  formatChatMessageTime(timestamp) {
    const date = new Date(String(timestamp || '').replace(' ', 'T'));
    if (!Number.isFinite(date.getTime())) return '';
    const now = new Date();
    const age = now.getTime() - date.getTime();
    const time = `${String(date.getHours()).padStart(2, '0')}:${String(date.getMinutes()).padStart(2, '0')}`;
    if (age <= 24 * 60 * 60 * 1000) return time;
    if (age <= 7 * 24 * 60 * 60 * 1000) {
      const weekdays = ['CN', 'T2', 'T3', 'T4', 'T5', 'T6', 'T7'];
      return `${time} ${weekdays[date.getDay()]}`;
    }
    return `${date.getDate()} tháng ${date.getMonth() + 1}, ${date.getFullYear()} ${time}`;
  }

  formatChatMessageTooltip(timestamp) {
    const date = new Date(String(timestamp || '').replace(' ', 'T'));
    if (!Number.isFinite(date.getTime())) return '';
    const weekdays = ['Chủ nhật', 'Thứ hai', 'Thứ ba', 'Thứ tư', 'Thứ năm', 'Thứ sáu', 'Thứ bảy'];
    const hour = date.getHours();
    const hour12 = hour % 12 || 12;
    const period = hour < 12 ? 'SA' : 'CH';
    const time = `${hour12}:${String(date.getMinutes()).padStart(2, '0')} ${period}`;
    if (Date.now() - date.getTime() > 7 * 24 * 60 * 60 * 1000) {
      return `${date.getDate()} tháng ${date.getMonth() + 1}, ${date.getFullYear()} ${time}`;
    }
    return `${weekdays[date.getDay()]} ${time}`;
  }

  openMessageReactionsModal(msgId, event) {
    if (event) event.stopPropagation();
    const message = this.msgCache[msgId];
    if (!message) return;
    const reactions = Array.isArray(message.reactions) ? message.reactions : [];
    if (!reactions.some(reaction => Array.isArray(reaction.users) && reaction.users.length)) {
      this.showToast('Không có thông tin người thả cảm xúc cho tin nhắn này.', 'error');
      return;
    }
    this.currentMessageReactions = reactions;
    let modal = document.getElementById('messageReactionsModal');
    if (!modal) {
      modal = document.createElement('div');
      modal.id = 'messageReactionsModal';
      modal.className = 'modal-overlay message-reactions-modal';
      modal.style.display = 'none';
      modal.innerHTML = `
        <div class="auth-card message-reactions-card" role="dialog" aria-modal="true" aria-label="Biểu cảm tin nhắn">
          <div class="message-reactions-header">
            <div class="reaction-tabs-bar" id="messageReactionsTabs"></div>
            <button type="button" class="btn-icon message-reactions-close" aria-label="Đóng">✕</button>
          </div>
          <div class="message-reactions-user-list" id="messageReactionsUserList"></div>
        </div>`;
      modal.addEventListener('click', event => {
        if (event.target === modal) this.closeMessageReactionsModal();
      });
      modal.querySelector('.message-reactions-card').addEventListener('click', event => event.stopPropagation());
      modal.querySelector('.message-reactions-close').addEventListener('click', () => this.closeMessageReactionsModal());
      modal.querySelector('#messageReactionsUserList').addEventListener('click', event => {
        const user = event.target.closest('[data-user-token]');
        if (user?.dataset.userToken) this.openUserProfile(user.dataset.userToken);
      });
      document.body.appendChild(modal);
    }
    const total = reactions.reduce((sum, reaction) => sum + (Number(reaction.count) || 0), 0);
    const tabs = [{ emoji: 'ALL', label: 'Tất cả', count: total }]
      .concat([...reactions].sort((a, b) => (Number(b.count) || 0) - (Number(a.count) || 0))
        .map(reaction => ({ emoji: reaction.emoji, label: reaction.emoji, count: Number(reaction.count) || 0 })));
    const tabsBox = modal.querySelector('#messageReactionsTabs');
    tabsBox.innerHTML = tabs.map((tab, index) => `
      <button type="button" class="reaction-tab-btn${index === 0 ? ' active' : ''}" data-emoji="${this.escapeHtml(tab.emoji)}">
        <span>${this.escapeHtml(tab.label)}</span><span class="message-reactions-tab-count">${tab.count}</span>
      </button>`).join('');
    tabsBox.querySelectorAll('.reaction-tab-btn').forEach(button => {
      button.addEventListener('click', () => {
        tabsBox.querySelectorAll('.reaction-tab-btn').forEach(tab => tab.classList.toggle('active', tab === button));
        this.renderMessageReactionsList(button.dataset.emoji);
      });
    });
    modal.style.display = 'flex';
    this.renderMessageReactionsList('ALL');
  }

  renderMessageReactionsList(emoji = 'ALL') {
    const list = document.getElementById('messageReactionsUserList');
    if (!list) return;
    const users = (this.currentMessageReactions || [])
      .filter(reaction => emoji === 'ALL' || reaction.emoji === emoji)
      .flatMap(reaction => (reaction.users || []).map(user => ({ ...user, emoji: reaction.emoji })));
    list.innerHTML = users.length ? users.map(user => `
      <div class="reaction-user-row" data-user-token="${this.escapeHtml(user.user_token || user.user_id || '')}">
        <div class="reaction-user-left">
          <div class="reaction-avatar-box">
            <img src="${this.escapeHtml(user.avatar || '/img/default-avatar.svg')}" alt="Avatar">
            <span class="reaction-avatar-badge">${this.escapeHtml(user.emoji)}</span>
          </div>
          <div class="message-reaction-user-name">${this.escapeHtml(user.name || 'Người dùng')}</div>
        </div>
      </div>`).join('') : '<div class="message-reactions-empty">Chưa có biểu cảm nào</div>';
  }

  closeMessageReactionsModal() {
    const modal = document.getElementById('messageReactionsModal');
    if (modal) modal.style.display = 'none';
  }

  // --- QUẢN LÝ HÀNH ĐỘNG TIN NHẮN (Hình 2) ---
  toggleMessageMoreMenu(msgId, event) {
    if (event) event.stopPropagation();
    const dropdown = document.getElementById(`msg-more-${msgId}`);
    if (!dropdown) return;
    const isShown = dropdown.classList.contains('show');
    this.closeAllMessageDropdowns();
    if (!isShown) {
      dropdown.classList.add('show');
      const row = dropdown.closest('.message-row');
      if (row) row.classList.add('active-dropdown');

      // Tự động kiểm tra vị trí: Nếu là tin nhắn gần đầu khung chat (khoảng cách đến đỉnh chatBody < 120px)
      // thì mở menu hướng xuống dưới (open-down) để không bị thanh header phía trên đè mất
      const chatBody = dropdown.closest('.chat-body');
      if (chatBody) {
        const bodyRect = chatBody.getBoundingClientRect();
        const btn = dropdown.parentElement;
        const btnRect = btn.getBoundingClientRect();
        if (btnRect.top - bodyRect.top < 120) {
          dropdown.classList.add('open-down');
        } else {
          dropdown.classList.remove('open-down');
        }
      }
    }
  }

  toggleMessageReactionPicker(msgId, event) {
    if (event) event.stopPropagation();
    const picker = document.getElementById(`msg-react-picker-${msgId}`);
    if (!picker) return;
    const wasOpen = picker.classList.contains('show');
    this.closeAllMessageDropdowns();
    if (wasOpen) return;
    picker.innerHTML = `${this.getQuickReactionValues().map(emoji =>
      `<button type="button" class="quick-reaction-option" title="Thả ${this.escapeHtml(emoji)}" onclick="app.addQuickReactionToMessage(${msgId}, ${app.jsArg(emoji)})">${emoji}</button>`
    ).join('')}<button type="button" class="quick-reaction-add" title="Thêm biểu tượng" onclick="app.openQuickReactionModal(${msgId})">＋</button>`;
    picker.classList.add('show');
    const row = picker.closest('.message-row');
    if (row) row.classList.add('active-dropdown');
    this.positionReactionPicker(picker, event?.currentTarget);
  }

  addQuickReactionToMessage(msgId, emoji) {
    this.closeAllMessageDropdowns();
    this.reactToChatMessage(msgId, emoji);
  }

  openQuickReactionModal(msgId) {
    this.closeAllMessageDropdowns();
    this.openChatEmojiModal('quick-reactions-edit', msgId);
  }

  positionReactionPicker(picker, trigger) {
    if (!picker || !trigger) return;
    const origin = picker.parentElement;
    if (origin && origin !== document.body) {
      picker.dataset.originWrapperId = origin.id || picker.dataset.originWrapperId || '';
    }
    picker.classList.add('is-floating');
    document.body.appendChild(picker);

    const triggerRect = trigger.getBoundingClientRect();
    const pickerRect = picker.getBoundingClientRect();
    const margin = 8;
    const left = Math.max(margin, Math.min(
      triggerRect.left + (triggerRect.width - pickerRect.width) / 2,
      window.innerWidth - pickerRect.width - margin
    ));
    let top = triggerRect.top - pickerRect.height - margin;
    if (top < margin) top = triggerRect.bottom + margin;
    top = Math.max(margin, Math.min(top, window.innerHeight - pickerRect.height - margin));
    picker.style.left = `${left}px`;
    picker.style.top = `${top}px`;
  }

  renderChatEmojiPickerContent(pickerId) {
    const categories = Object.entries(CHAT_EMOJI_CATEGORIES);
    return `
      <div class="chat-emoji-picker-content">
        <label class="chat-emoji-search"><span>⌕</span>
          <input type="search" placeholder="Tìm kiếm biểu tượng cảm xúc" oninput="app.filterChatEmojiPicker('${pickerId}', this.value)">
        </label>
        <div class="chat-emoji-category-title"></div>
        <div class="chat-emoji-grid"></div>
        <div class="chat-emoji-category-tabs">
          <button type="button" class="active" data-category="recent" title="Đã dùng gần đây" onclick="app.selectChatEmojiCategory(${app.jsArg(pickerId)}, 'recent')">🕘</button>
          ${categories.map(([key, category]) => `<button type="button" data-category="${key}" title="${category.label}" onclick="app.selectChatEmojiCategory(${app.jsArg(pickerId)}, ${app.jsArg(key)})">${category.icon}</button>`).join('')}
        </div>
      </div>
    `;
  }

  selectChatEmojiCategory(pickerId, category) {
    const picker = document.getElementById(pickerId);
    if (!picker) return;
    picker.dataset.category = category;
    picker.querySelectorAll('.chat-emoji-category-tabs button').forEach(button => {
      button.classList.toggle('active', button.dataset.category === category);
    });
    const query = picker.querySelector('.chat-emoji-search input')?.value || '';
    this.filterChatEmojiPicker(pickerId, query);
  }

  filterChatEmojiPicker(pickerId, query = '') {
    const picker = document.getElementById(pickerId);
    if (!picker) return;
    const categoryKey = picker.dataset.category || 'recent';
    const recent = JSON.parse(localStorage.getItem('cs_recent_chat_emojis') || '[]');
    let emojis = categoryKey === 'recent'
      ? recent.map(value => [value, 'Đã dùng gần đây'])
      : (CHAT_EMOJI_CATEGORIES[categoryKey]?.emojis || '').split(/\s+/).filter(Boolean).map(value => [value, CHAT_EMOJI_CATEGORIES[categoryKey].label]);
    const normalizedQuery = this.removeVietnameseTonesClient(query.trim());
    if (normalizedQuery) {
      const allEmojis = Object.values(CHAT_EMOJI_CATEGORIES)
        .flatMap(category => category.emojis.split(/\s+/).filter(Boolean).map(value => [value, category.label]));
      emojis = allEmojis.filter(([emoji, category]) =>
        emoji.includes(query.trim()) ||
        this.removeVietnameseTonesClient(`${CHAT_EMOJI_SEARCH_NAMES[emoji] || ''} ${category}`).includes(normalizedQuery)
      );
    }

    const title = picker.querySelector('.chat-emoji-category-title');
    const grid = picker.querySelector('.chat-emoji-grid');
    if (title) title.textContent = normalizedQuery ? 'Kết quả tìm kiếm' : (categoryKey === 'recent' ? 'Đã dùng gần đây' : CHAT_EMOJI_CATEGORIES[categoryKey]?.label || '');
    if (grid) {
      grid.innerHTML = emojis.length
        ? emojis.map(([emoji, label]) => `<button type="button" class="chat-emoji-option" title="${this.escapeHtml(label)}" onclick="app.selectChatEmoji(${app.jsArg(pickerId)}, ${app.jsArg(emoji)})">${emoji}</button>`).join('')
        : '<div class="chat-emoji-empty">Chưa có biểu tượng gần đây. Hãy chọn một danh mục.</div>';
    }
  }

  selectChatEmoji(pickerId, emoji) {
    const picker = document.getElementById(pickerId);
    if (!picker) return;
    const recent = JSON.parse(localStorage.getItem('cs_recent_chat_emojis') || '[]');
    localStorage.setItem('cs_recent_chat_emojis', JSON.stringify([emoji, ...recent.filter(item => item !== emoji)].slice(0, 24)));
    if (picker.dataset.mode === 'message') {
      const targetId = Number(picker.dataset.targetId);
      this.closeChatEmojiModal();
      this.reactToChatMessage(targetId, emoji);
      return;
    }
    if (picker.dataset.mode === 'quick-reactions-edit') {
      const targetId = Number(picker.dataset.targetId);
      this.changeQuickReaction(emoji, targetId);
      return;
    }
    if (picker.dataset.mode === 'quick-send') {
      this.closeChatEmojiModal();
      this.sendChatMessage({
        content: emoji,
        mediaType: 'EMOJI_1',
        preservePendingMedia: !!this.pendingChatMedia?.url
      });
      return;
    }

    const input = document.getElementById('chatInput');
    if (input) {
      const start = input.selectionStart ?? input.value.length;
      const end = input.selectionEnd ?? start;
      input.setRangeText(emoji, start, end, 'end');
      input.focus();
    }
    this.closeChatEmojiModal();
  }

  toggleComposerEmojiPicker(event) {
    if (event) event.stopPropagation();
    if (document.getElementById('chatEmojiModalOverlay')) {
      this.closeChatEmojiModal();
      return;
    }
    this.openChatEmojiModal('composer');
  }

  openChatEmojiModal(mode = 'composer', targetId = '') {
    this.closeChatEmojiModal();
    this.closeAllMessageDropdowns();
    const overlay = document.createElement('div');
    overlay.id = 'chatEmojiModalOverlay';
    overlay.className = 'chat-emoji-modal-overlay';
    overlay.innerHTML = `
      <section class="chat-emoji-modal" role="dialog" aria-modal="true" aria-labelledby="chatEmojiModalTitle">
        <header class="chat-emoji-modal-header">
          <span id="chatEmojiModalTitle">Biểu tượng cảm xúc</span>
          <button type="button" class="chat-emoji-modal-close" aria-label="Đóng" onclick="app.closeChatEmojiModal()">×</button>
        </header>
        <div class="chat-emoji-modal-picker" id="chatEmojiPickerModal"></div>
      </section>
    `;
    overlay.addEventListener('click', event => {
      if (event.target === overlay) this.closeChatEmojiModal();
    });
    document.body.appendChild(overlay);
    const picker = document.getElementById('chatEmojiPickerModal');
    if (!picker) return;
    picker.dataset.mode = mode;
    picker.dataset.targetId = String(targetId);
    picker.dataset.category = 'recent';
    picker.innerHTML = this.renderChatEmojiPickerContent(picker.id);
    this.filterChatEmojiPicker(picker.id);
    picker.querySelector('.chat-emoji-search input')?.focus();
  }

  closeChatEmojiModal() {
    document.getElementById('chatEmojiModalOverlay')?.remove();
  }

  sendChatLike(size = 1) {
    const input = document.getElementById('chatInput');
    if (!input) return;
    const emoji = this.quickReactionEmoji || '👍';
    if (input.value.trim() || this.pendingChatMedia?.url) {
      if (!input.value.trim()) input.value = emoji;
      this.sendChatMessage();
    } else {
      this.sendChatMessage({ content: emoji, mediaType: `EMOJI_${size}` });
    }
  }

  closeAllMessageDropdowns() {
    document.querySelectorAll('.msg-more-dropdown.show, .msg-reaction-picker.show').forEach(el => {
      el.classList.remove('show');
      el.classList.remove('open-down');
    });
    document.querySelectorAll('.msg-reaction-picker.is-floating').forEach(picker => {
      const wrapper = picker.dataset.originWrapperId
        ? document.getElementById(picker.dataset.originWrapperId)
        : null;
      if (wrapper) wrapper.appendChild(picker);
      else picker.remove();
      picker.classList.remove('is-floating');
      picker.style.removeProperty('left');
      picker.style.removeProperty('top');
    });
    document.getElementById('chatEmojiPickerComposer')?.classList.remove('open');
    document.querySelectorAll('.message-row.active-dropdown').forEach(r => {
      r.classList.remove('active-dropdown');
    });
  }

  async reactToChatMessage(msgId, reaction) {
    const scrollAnchor = this.captureMessageScrollAnchor(msgId);
    this.messageScrollAnchor = scrollAnchor;
    this.closeAllMessageDropdowns();
    const isGroup = this.currentChatPartner && this.currentChatPartner.isGroup;
    const url = isGroup ? '/api/groups/messages/react' : '/api/messages/react';
    try {
      const res = await fetch(url, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ message_id: msgId, user_id: this.currentUser.id, reaction: reaction })
      });
      const data = await res.json();
      if (data.status === 'ok') {
        if (isGroup) await this.loadGroupMessages(this.currentChatPartner.id);
        else await this.loadInitialMessages();
        requestAnimationFrame(() => this.restoreMessageScrollAnchor(scrollAnchor));
      } else {
        this.showToast(data.message || 'Không thể thả cảm xúc tin nhắn!', 'error');
      }
    } catch (e) {
      console.error('Lỗi thả cảm xúc tin nhắn:', e);
      this.showToast('Không thể thả cảm xúc tin nhắn!', 'error');
    } finally {
      window.setTimeout(() => {
        if (this.messageScrollAnchor === scrollAnchor) this.messageScrollAnchor = null;
      }, 2500);
    }
  }

  startReplyToMessage(msgId, authorName, snippet) {
    this.closeAllMessageDropdowns();
    const m = this.msgCache[msgId];
    const isMe = m && this.currentUser && m.sender_id === this.currentUser.id;
    const author = authorName || (m && m.sender_name) || (isMe ? 'Bạn' : 'Bạn bè');
    // Chỉ lưu/hiển thị đoạn ngắn của tin nhắn gốc
    const shortText = m ? this.getReplySnippetForMessage(m) : (this.makeReplySnippet(snippet) || 'Đính kèm');
    this.replyingToMessage = { id: msgId, author: author, content: shortText };
    const bar = document.getElementById('chatReplyBar');
    const authorEl = document.getElementById('chatReplyAuthor');
    const snipEl = document.getElementById('chatReplySnippet');
    if (bar && authorEl && snipEl) {
      authorEl.textContent = author;
      snipEl.textContent = shortText;
      bar.style.display = 'flex';
    }
    const input = document.getElementById('chatInput');
    if (input) input.focus();
  }

  cancelChatReply() {
    this.replyingToMessage = null;
    const bar = document.getElementById('chatReplyBar');
    if (bar) bar.style.display = 'none';
  }

  handleRecallClick(msgId, createdAt) {
    this.closeAllMessageDropdowns();
    if (createdAt) {
      const msgTime = new Date(createdAt.replace(' ', 'T')).getTime();
      const now = Date.now();
      if ((now - msgTime) > 5 * 60 * 1000) {
        // Quá 5 phút: HIỂN THỊ THÔNG BÁO GIỮA MÀN HÌNH (Hình 2 theo yêu cầu)
        this.showCenterAlertModal(
          'Không thể thu hồi tin nhắn',
          'Tin nhắn đã gửi quá 5 phút nên không thể thu hồi cho mọi người.',
          '⚠️'
        );
        return;
      }
    }
    this.recallMessage(msgId);
  }

  reportMessage(msgId, senderId) {
    this.closeAllMessageDropdowns();
    this.openReportModal(senderId, '', msgId);
  }

  showCenterAlertModal(title, message, icon = '⚠️') {
    const modal = document.getElementById('centerScreenModal');
    if (!modal) {
      alert(message);
      return;
    }
    const iconEl = document.getElementById('centerAlertIcon');
    const titleEl = document.getElementById('centerAlertTitle');
    const msgEl = document.getElementById('centerAlertMessage');
    if (iconEl) iconEl.textContent = icon;
    if (titleEl) titleEl.textContent = title;
    if (msgEl) msgEl.textContent = message;
    modal.style.display = 'flex';
  }

  closeCenterAlertModal() {
    const modal = document.getElementById('centerScreenModal');
    if (modal) modal.style.display = 'none';
  }

  // --- THU HỒI & XÓA TIN NHẮN (Item 9) ---
  async recallMessage(msgId) {
    const confirmed = await this.showConfirm('Thu hồi tin nhắn này với mọi người?');
    if (!confirmed) return;
    const isGroup = this.currentChatPartner && this.currentChatPartner.isGroup;
    const url = isGroup ? '/api/groups/messages/recall' : '/api/messages/recall';
    try {
      const res = await fetch(url, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          message_id: msgId,
          sender_id: this.currentUser.id
        })
      });
      const data = await res.json();
      if (data.status === 'ok') {
        this.showToast('Đã thu hồi tin nhắn!', 'success');
        if (isGroup) this.loadGroupMessages(this.currentChatPartner.id);
        else this.loadInitialMessages();
        this.loadFriendsAndContacts();
      } else {
        this.showToast(data.message || 'Không thể thu hồi!', 'error');
      }
    } catch (e) {
      this.showToast('Lỗi: ' + e.message, 'error');
    }
  }



  async deleteMessage(msgId) {
    this.closeAllMessageDropdowns();
    const confirmed = await this.showConfirm('Xóa tin nhắn này ở phía bạn?');
    if (!confirmed) return;
    const isGroup = this.currentChatPartner && this.currentChatPartner.isGroup;
    const url = isGroup ? '/api/groups/messages/delete' : '/api/messages/delete';
    try {
      const res = await fetch(url, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          message_id: msgId,
          user_id: this.currentUser.id
        })
      });
      const data = await res.json();
      if (data.status === 'ok') {
        const row = document.getElementById(`msg-${msgId}`);
        if (row) row.remove();
        delete this.msgCache[msgId];
        this.showToast('Đã xóa tin nhắn!', 'success');
        this.loadFriendsAndContacts();
      } else {
        this.showToast(data.message || 'Không thể xóa tin nhắn.', 'error');
      }
    } catch (e) {
      this.showToast('Không thể xóa tin nhắn: ' + e.message, 'error');
    }
  }

  async sendChatMessage(options = {}) {
    const input = document.getElementById('chatInput');
    if (!input || !this.currentChatPartner || !this.currentUser) return;
    const text = options.content ?? input.value.trim();
    const media = options.mediaType
      ? { type: options.mediaType, url: '' }
      : this.pendingChatMedia || { type: 'NONE', url: '' };
    if (!text && !media.url) return;

    const replyText = this.replyingToMessage ? this.replyingToMessage.content : '';
    const replyId = this.replyingToMessage ? this.replyingToMessage.id : 0;
    const pendingRow = this.appendPendingChatMessage(text, media, replyText, replyId);

    if (this.currentChatPartner.isGroup) {
      // Nhắn tin nhóm
      try {
        const res = await fetch('/api/groups/messages/send', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({
            group_id: this.currentChatPartner.id,
            sender_id: this.currentUser.id,
            content: text,
            media_type: media.type,
            media_url: media.url,
            reply_to_text: replyText,
            reply_to_id: replyId
          })
        });
        const data = await res.json();
        if (!res.ok || data.status !== 'ok') {
          pendingRow?.remove();
          this.showToast(data.message || 'Không thể gửi tin nhắn vào nhóm.', 'error');
          return;
        }
        input.value = '';
        this.cancelChatReply();
        if (!options.preservePendingMedia) this.clearPendingChatMedia();
        await this.loadGroupMessages(this.currentChatPartner.id);
        this.loadFriendsAndContacts();
      } catch (e) {
        pendingRow?.remove();
        console.error('Lỗi gửi tin nhắn nhóm:', e);
        this.showToast(`Không thể gửi tin nhắn: ${e.message}`, 'error');
      }
    } else {
      // Nhắn tin 1-on-1
      try {
        const res = await fetch('/api/messages/send', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({
            sender_id: this.currentUser.id,
            recipient_id: this.currentChatPartner.id,
            content: text,
            media_type: media.type,
            media_url: media.url,
            reply_to_text: replyText,
            reply_to_id: replyId
          })
        });
        const data = await res.json();
        if (!res.ok || data.status !== 'ok') {
          pendingRow?.remove();
          this.showToast(data.message || 'Không thể gửi tin nhắn.', 'error');
          return;
        }
        input.value = '';
        this.cancelChatReply();
        this.stopTyping();
        if (!options.preservePendingMedia) this.clearPendingChatMedia();
        await this.loadInitialMessages();
        this.loadFriendsAndContacts();
      } catch (e) {
        pendingRow?.remove();
        console.error('Lỗi gửi tin nhắn:', e);
        this.showToast(`Không thể gửi tin nhắn: ${e.message}`, 'error');
      }
    }
  }

  appendPendingChatMessage(content, media, replyText, replyId) {
    const list = document.getElementById('messagesList');
    const chatBody = document.getElementById('chatBody');
    if (!list) return null;
    if (!list.querySelector('.message-row')) list.innerHTML = '';
    const message = {
      id: 0,
      sender_id: this.currentUser.id,
      content,
      media_type: media.type || 'NONE',
      media_url: media.url || '',
      reply_to_text: replyText,
      reply_to_id: replyId,
      created_at: new Date().toISOString().slice(0, 19).replace('T', ' ')
    };
    const row = document.createElement('div');
    row.className = 'message-row me msg-cluster-single pending-message-row';
    row.id = `pending-message-${Date.now()}-${Math.random().toString(36).slice(2, 7)}`;
    row.innerHTML = `
      <div class="message-main">
        <div class="message-content-wrapper">
          <div class="message-bubble-container">
            <div class="message-bubble">${this.renderMessageContent(message)}</div>
          </div>
        </div>
      </div>
      <div class="message-receipt-status"><span class="message-delivery-label">Đang gửi</span></div>
    `;
    list.appendChild(row);
    if (chatBody) chatBody.scrollTop = chatBody.scrollHeight;
    return row;
  }

  async chatActionClearMessages() {
    if (!this.currentChatPartner) return;
    if (this.currentChatPartner.isGroup) {
      this.chatActionClearGroupMessages();
      return;
    }
    const _c6 = await this.showConfirm('Xóa toàn bộ tin nhắn với người này?'); if (!_c6) return;
    try {
      const res = await fetch('/api/messages/clear', {
        method: 'DELETE',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ my_id: this.currentUser.id, partner_id: this.currentChatPartner.id })
      });
      const data = await res.json();
      if (data.status === 'ok') {
        this.showToast('Đã xóa đoạn tin nhắn!', 'success');
        document.getElementById('messagesList').innerHTML = '<div style="text-align: center; font-size: 0.8rem; color: var(--text-muted); padding: 1.5rem;">Đoạn trò chuyện đã được xóa.</div>';
        this.toggleChatMenu();
        this.loadFriendsAndContacts();
      }
    } catch (e) {}
  }

  async chatActionBlockUser() {
    if (!this.currentChatPartner || this.currentChatPartner.isGroup) return;
    const _cb = await this.showConfirm(`Chặn ${this.currentChatPartner.name}?`);
    if (!_cb) return;
    try {
      const res = await fetch('/api/users/block', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ my_id: this.currentUser.id, target_id: this.currentChatPartner.id })
      });
      const data = await res.json();
      if (data.status === 'ok') {
        this.showToast('Đã chặn người dùng!', 'success');
        this.closeChat();
        this.loadFriendsAndContacts();
      }
    } catch (e) {}
  }

  chatActionReportUser() {
    if (!this.currentChatPartner) return;
    this.openReportModal(this.currentChatPartner.id, '');
    this.toggleChatMenu();
  }

  chatActionCreateGroup() {
    const preSelected = this.currentChatPartner && !this.currentChatPartner.isGroup
      ? [this.currentChatPartner.id] : [];
    this.openCreateGroupModal(preSelected);
    this.toggleChatMenu();
  }

  // --- TẠO NHÓM MỚI ---
  async openCreateGroupModal(preSelectedIds = []) {
    // Load danh sách bạn bè để chọn thành viên
    let friends = [];
    try {
      const res = await fetch(`/api/friends/contacts?user_id=${encodeURIComponent(this.currentUser ? this.currentUser.id : '')}`);
      const data = await res.json();
      if (data.status === 'ok') friends = data.contacts || [];
    } catch (_) {}

    const listEl = document.getElementById('groupInviteFriendsList');
    if (listEl) {
      if (friends.length === 0) {
        listEl.innerHTML = '<div style="text-align:center;color:var(--text-muted);padding:1rem;font-size:0.85rem;">Chưa có bạn bè nào để thêm</div>';
      } else {
        listEl.innerHTML = friends.map(f => {
          const checked = preSelectedIds.includes(f.id) ? 'checked' : '';
          return `
            <label style="display:flex;align-items:center;gap:0.6rem;padding:0.4rem 0.2rem;cursor:pointer;">
              <input type="checkbox" class="group-invite-checkbox" value="${f.id}" ${checked} style="width:16px;height:16px;cursor:pointer;">
              <img src="${app.escapeHtml(f.avatar || '/img/default-avatar.svg')}" style="width:32px;height:32px;border-radius:50%;object-fit:cover;">
              <span style="font-size:0.88rem;font-weight:500;">${this.escapeHtml(f.name || f.nickname || '')}</span>
            </label>
          `;
        }).join('');
      }
    }

    // Reset input tên nhóm
    const nameEl = document.getElementById('newGroupName');
    if (nameEl) nameEl.value = '';

    const modal = document.getElementById('createGroupModal');
    if (modal) modal.style.display = 'flex';
  }

  closeCreateGroupModal() {
    const modal = document.getElementById('createGroupModal');
    if (modal) modal.style.display = 'none';
  }

  async submitCreateGroup() {
    const nameEl = document.getElementById('newGroupName');
    const descEl = document.getElementById('newGroupDesc');
    const groupName = (nameEl?.value || '').trim();
    if (!groupName) {
      this.showToast('Vui lòng nhập tên nhóm!', 'error');
      return;
    }
    const checkboxes = document.querySelectorAll('.group-invite-checkbox:checked');
    const memberIds = Array.from(checkboxes).map(cb => cb.value);

    try {
      const res = await fetch('/api/groups/create', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          name: groupName,
          description: descEl?.value || '',
          created_by: this.currentUser ? this.currentUser.id : '',
          members: memberIds
        })
      });
      const data = await res.json();
      if (data.status === 'ok') {
        this.closeCreateGroupModal();
        this.showToast('Tạo nhóm thành công!', 'success');
        this.loadFriendsAndContacts();
        const grp = data.group;
        if (grp && grp.id) {
          this.openGroupChat(grp.id, grp.name || groupName, grp.avatar || '');
        }
      } else {
        this.showToast(data.message || 'Lỗi tạo nhóm!', 'error');
      }
    } catch (e) {
      this.showToast('Lỗi kết nối: ' + e.message, 'error');
    }
  }

  // --- MODAL BIỂU CẢM BÀI VIẾT (HÌNH 2) (Item 5) ---
  async openPostReactionsModal(postId) {
    try {
      const res = await fetch(`/api/posts/reactions-list?post_id=${encodeURIComponent(postId)}&user_id=${this.currentUser ? this.currentUser.id : ''}`);
      const data = await res.json();
      if (data.status === 'ok') {
        this.showReactionsListModal(data);
      }
    } catch (e) {
      console.error('Lỗi tải danh sách cảm xúc bài viết:', e);
      this.showToast('Không thể tải danh sách cảm xúc!', 'error');
    }
  }

  async openCommentReactionsModal(commentId) {
    try {
      const res = await fetch(`/api/comments/reactions-list?comment_id=${encodeURIComponent(commentId)}&user_id=${this.currentUser ? this.currentUser.id : ''}`);
      const data = await res.json();
      if (data.status === 'ok') {
        this.showReactionsListModal(data);
      } else {
        this.showToast(data.message || 'Không thể tải danh sách cảm xúc!', 'error');
      }
    } catch (error) {
      console.error('Lỗi tải danh sách cảm xúc bình luận:', error);
      this.showToast('Không thể tải danh sách cảm xúc!', 'error');
    }
  }

  showReactionsListModal(data) {
    this.currentReactionListData = data;
    const tabsBox = document.getElementById('postReactionsTabs');
    if (!tabsBox) return;
    const counts = data.counts || {};
    const tabs = [
      { type: 'ALL', label: 'Tất cả', count: counts.ALL || 0 },
      { type: 'LIKE', label: '👍', count: counts.LIKE || 0 },
      { type: 'LOVE', label: '❤️', count: counts.LOVE || 0 },
      { type: 'HAHA', label: '😆', count: counts.HAHA || 0 },
      { type: 'WOW', label: '😮', count: counts.WOW || 0 },
      { type: 'SAD', label: '😢', count: counts.SAD || 0 },
      { type: 'ANGRY', label: '😡', count: counts.ANGRY || 0 },
    ];
    tabsBox.innerHTML = tabs.filter(tab => tab.type === 'ALL' || tab.count > 0).map((tab, index) => `
      <button class="reaction-tab-btn ${index === 0 ? 'active' : ''}" onclick="app.filterReactionsList(${app.jsArg(tab.type)}, this)">
        <span>${tab.label}</span>
        <span style="font-size: 0.82rem; opacity: 0.85;">${tab.count}</span>
      </button>
    `).join('');
    this.renderReactionsUserList(data.users);
    const modal = document.getElementById('postReactionsModal');
    if (modal) modal.style.display = 'flex';
  }

  closePostReactionsModal() {
    const modal = document.getElementById('postReactionsModal');
    if (modal) modal.style.display = 'none';
  }

  filterReactionsList(type, btn) {
    document.querySelectorAll('.reaction-tab-btn').forEach(b => b.classList.remove('active'));
    if (btn) btn.classList.add('active');

    if (!this.currentReactionListData || !this.currentReactionListData.users) return;
    const filtered = (type === 'ALL') ? this.currentReactionListData.users : this.currentReactionListData.users.filter(u => u.type === type);
    this.renderReactionsUserList(filtered);
  }

  renderReactionsUserList(users) {
    const listEl = document.getElementById('postReactionsUserList');
    if (!listEl) return;
    if (!users || users.length === 0) {
      listEl.innerHTML = '<div style="text-align: center; color: var(--text-muted); padding: 1.5rem; font-size: 0.85rem;">Chưa có biểu cảm nào</div>';
      return;
    }
    listEl.innerHTML = users.map(u => {
      let actionBtn = '';
      if (u.relation === 'ME') {
        actionBtn = '';
      } else if (u.relation === 'FRIEND') {
        actionBtn = `<button class="btn-secondary" style="font-size: 0.82rem; padding: 4px 10px;" disabled>✓ Bạn bè</button>`;
      } else if (u.relation === 'PENDING_SENT') {
        actionBtn = `<button class="btn-secondary" style="font-size: 0.82rem; padding: 4px 10px;" disabled>⏳ Đã gửi</button>`;
      } else {
        actionBtn = `<button class="btn-primary" style="font-size: 0.82rem; padding: 4px 10px;" onclick="app.sendFriendRequest(${app.jsArg(u.user_id)}, this)">👤+ Thêm bạn bè</button>`;
      }

      return `
        <div class="reaction-user-row">
          <div class="reaction-user-left" onclick="app.openUserProfile(${app.jsArg(u.user_token || u.user_id)})">
            <div class="reaction-avatar-box">
              <img src="${app.escapeHtml(u.avatar || '/img/default-avatar.svg')}" alt="Avatar">
              <span class="reaction-avatar-badge">${u.icon || '👍'}</span>
            </div>
            <div style="font-weight: 600; font-size: 0.92rem; color: var(--text-primary);">${this.escapeHtml(u.name)}</div>
          </div>
          ${actionBtn}
        </div>
      `;
    }).join('');
  }

  // =========================================================================
  // 14. HỖ TRỢ (BÁO CÁO SỰ CỐ TỚI ADMIN) & BÁO CÁO VI PHẠM
  // =========================================================================
  // ===== SIDEBAR NAV =====
  toggleSidebarGroup(groupId) {
    const group = document.getElementById('navGroup' + groupId.charAt(0).toUpperCase() + groupId.slice(1));
    if (!group) return;
    const willOpen = !group.classList.contains('open');
    // optional: only one open at a time on narrow screens
    if (window.innerWidth < 1100) {
      document.querySelectorAll('.sidebar-nav-group.open').forEach(g => {
        if (g !== group) g.classList.remove('open');
      });
    }
    group.classList.toggle('open', willOpen);
  }

  async refreshSidebarFriendBadge() {
    if (!this.currentUser || this.currentUser.role === 'ADMIN') return;
    try {
      const res = await fetch(`/api/friends/requests?user_id=${encodeURIComponent(this.currentUser.id)}`);
      const data = await res.json();
      const count = (data.status === 'ok' && Array.isArray(data.requests)) ? data.requests.length : 0;
      ['sideFriendReqBadge', 'sideFriendReqSubBadge'].forEach(id => {
        const el = document.getElementById(id);
        if (!el) return;
        if (count > 0) {
          el.textContent = count > 99 ? '99+' : String(count);
          el.style.display = 'inline-flex';
        } else {
          el.style.display = 'none';
        }
      });
      const reqBadge = document.getElementById('reqCountBadge');
      if (reqBadge) reqBadge.textContent = String(count);
    } catch (e) { /* ignore */ }
  }

  openHelpCenter() {
    this._openInfoModal('📚 Trung tâm trợ giúp', `
      <ul style="line-height:1.7;padding-left:1.1rem;margin:0;color:var(--text-main);">
        <li><strong>Bảng tin:</strong> Xem và đăng bài viết, tương tác với bạn bè.</li>
        <li><strong>Bạn bè:</strong> Quản lý danh sách, lời mời và gợi ý kết bạn.</li>
        <li><strong>Nhóm:</strong> Tham gia cộng đồng, đăng bài trong nhóm.</li>
        <li><strong>Tin nhắn:</strong> Trò chuyện trực tiếp hoặc trong nhóm chat.</li>
        <li><strong>Hỗ trợ:</strong> Báo cáo sự cố hoặc nội dung vi phạm tới quản trị viên.</li>
      </ul>
      <p style="margin-top:1rem;color:var(--text-muted);font-size:.9rem;">Nếu cần hỗ trợ thêm, hãy dùng mục <em>Báo cáo sự cố</em> hoặc <em>Liên hệ quản trị viên</em>.</p>
    `);
  }

  openFaqModal() {
    this._openInfoModal('❓ Câu hỏi thường gặp (FAQ)', `
      <div style="display:flex;flex-direction:column;gap:.9rem;color:var(--text-main);font-size:.92rem;">
        <div><strong>Làm sao để kết bạn?</strong><br><span style="color:var(--text-muted);">Vào Bạn bè → Gợi ý kết bạn, hoặc tìm người dùng rồi nhấn Kết bạn.</span></div>
        <div><strong>Làm sao tạo nhóm?</strong><br><span style="color:var(--text-muted);">Vào Nhóm → Tạo nhóm mới, đặt tên và quyền riêng tư.</span></div>
        <div><strong>Không gửi được tin nhắn?</strong><br><span style="color:var(--text-muted);">Có thể bạn hoặc đối phương đã chặn nhau. Kiểm tra danh sách đã chặn.</span></div>
        <div><strong>Cách báo cáo nội dung xấu?</strong><br><span style="color:var(--text-muted);">Dùng Hỗ trợ → Báo cáo vi phạm, hoặc nút báo cáo trên bài viết/tin nhắn.</span></div>
        <div><strong>Quên mật khẩu?</strong><br><span style="color:var(--text-muted);">Liên hệ quản trị viên qua mục Hỗ trợ để được hỗ trợ đặt lại.</span></div>
      </div>
    `);
  }

  openContactAdmin() {
    this.openSupportModal();
    const sel = document.getElementById('supportCategorySelect');
    if (sel) {
      // Prefer a matching option if present
      for (const opt of sel.options) {
        if (/quản trị|admin|hỗ trợ khác|ý kiến/i.test(opt.value)) { sel.value = opt.value; break; }
      }
    }
    const heading = document.querySelector('#supportModal h2');
    if (heading) heading.textContent = '✉️ Liên hệ quản trị viên';
  }

  openUserGuide() {
    this._openInfoModal('📖 Hướng dẫn sử dụng website', `
      <ol style="line-height:1.75;padding-left:1.15rem;margin:0;color:var(--text-main);font-size:.92rem;">
        <li>Đăng nhập hoặc đăng ký tài khoản.</li>
        <li>Cập nhật hồ sơ (avatar, quê quán, trường học, sở thích).</li>
        <li>Kết bạn từ mục <strong>Bạn bè</strong> hoặc gợi ý trên bảng tin.</li>
        <li>Đăng bài viết trên <strong>Bảng tin chính</strong> (có thể kèm ảnh/video).</li>
        <li>Nhắn tin qua biểu tượng 💬 hoặc danh sách đoạn chat bên phải.</li>
        <li>Tham gia / tạo <strong>Nhóm</strong> cộng đồng để chia sẻ theo chủ đề.</li>
        <li>Dùng <strong>Hỗ trợ</strong> khi gặp sự cố hoặc cần báo cáo vi phạm.</li>
      </ol>
    `);
  }

  _openInfoModal(title, bodyHtml) {
    let modal = document.getElementById('infoHelpModal');
    if (!modal) {
      modal = document.createElement('div');
      modal.id = 'infoHelpModal';
      modal.className = 'modal-overlay';
      modal.style.display = 'none';
      modal.innerHTML = `
        <div class="auth-card" style="max-width:560px;max-height:85vh;overflow:auto;">
          <div class="auth-header" style="display:flex;justify-content:space-between;align-items:flex-start;gap:1rem;">
            <h2 id="infoHelpTitle" style="font-size:1.2rem;margin:0;"></h2>
            <button class="btn-icon" onclick="app.closeInfoHelpModal()" title="Đóng">✕</button>
          </div>
          <div id="infoHelpBody" style="margin-top:.75rem;"></div>
          <div style="margin-top:1.2rem;display:flex;justify-content:flex-end;">
            <button class="btn-primary" onclick="app.closeInfoHelpModal()">Đã hiểu</button>
          </div>
        </div>`;
      modal.addEventListener('click', (e) => { if (e.target === modal) this.closeInfoHelpModal(); });
      document.body.appendChild(modal);
    }
    document.getElementById('infoHelpTitle').textContent = title;
    document.getElementById('infoHelpBody').innerHTML = bodyHtml;
    modal.style.display = 'flex';
  }

  closeInfoHelpModal() {
    const modal = document.getElementById('infoHelpModal');
    if (modal) modal.style.display = 'none';
  }

  openSupportModal() {
    const modal = document.getElementById('supportModal');
    if (modal) {
      document.getElementById('supportDetailInput').value = '';
      modal.style.display = 'flex';
    }
  }

  closeSupportModal() {
    const modal = document.getElementById('supportModal');
    if (modal) modal.style.display = 'none';
  }

  async submitSupportReport() {
    if (!this.currentUser) return;
    const cat = document.getElementById('supportCategorySelect').value;
    const detail = document.getElementById('supportDetailInput').value.trim();

    if (!detail) {
      this.showToast('Vui lòng mô tả chi tiết sự cố bạn gặp phải!', 'error');
      return;
    }

    try {
      const res = await fetch('/api/reports/create', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          reporter_id: this.currentUser.id,
          reason: `[HỖ TRỢ / BÁO CÁO SỰ CỐ - ${cat}]: ${detail}`
        })
      });
      const data = await res.json();
      if (data.status === 'ok') {
        this.showToast('Báo cáo sự cố đã được gửi tới Quản trị viên!', 'success');
        this.closeSupportModal();
      } else {
        this.showToast(data.message || 'Lỗi gửi báo cáo!', 'error');
      }
    } catch (err) {
      this.showToast('Lỗi: ' + err.message, 'error');
    }
  }

  injectReportModal() {
    if (document.getElementById('reportModal')) return;
    const modalDiv = document.createElement('div');
    modalDiv.className = 'modal-overlay';
    modalDiv.id = 'reportModal';
    modalDiv.style.display = 'none';
    modalDiv.innerHTML = `
      <div class="auth-card" style="max-width: 420px; width: 92%;">
        <div class="auth-header" style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 0.8rem;">
          <h2 id="reportModalHeading" style="font-size: 1.25rem;">🚩 Báo cáo vi phạm</h2>
          <button type="button" class="btn-close-modal" onclick="app.closeReportModal()" style="background: none; border: none; font-size: 1.25rem; cursor: pointer; color: var(--text-muted);" title="Đóng">✖</button>
        </div>
        <p id="reportModalSubHeading" style="font-size: 0.82rem; color: var(--text-muted); margin-bottom: 1rem;">Báo cáo sẽ được chuyển thẳng tới Quản trị viên hệ thống xử lý</p>
        <div class="form-group" style="margin-bottom: 0.75rem;">
          <label style="font-weight: 600; font-size: 0.85rem; margin-bottom: 0.35rem; display: block;">Lý do báo cáo:</label>
          <select id="reportReasonSelect" style="width: 100%; padding: 0.6rem 0.8rem; border-radius: 8px; border: 1px solid var(--border-color); background: var(--bg-surface-secondary); color: var(--text-primary); font-size: 0.9rem;">
            <option value="Spam / Tin rác">Spam / Tin rác</option>
            <option value="Nội dung thô lỗ, xúc phạm">Nội dung thô lỗ, xúc phạm</option>
            <option value="Tài khoản giả mạo người khác">Tài khoản giả mạo người khác</option>
            <option value="Hành vi lừa đảo">Hành vi lừa đảo</option>
            <option value="Khác">Lý do khác...</option>
          </select>
        </div>
        <div class="form-group" style="margin-bottom: 1rem;">
          <label style="font-weight: 600; font-size: 0.85rem; margin-bottom: 0.35rem; display: block;">Chi tiết thêm:</label>
          <textarea id="reportDetailInput" style="width: 100%; border-radius: 8px; padding: 0.6rem 0.8rem; background: var(--bg-surface-secondary); color: var(--text-primary); border: 1px solid var(--border-color); font-size: 0.9rem;" placeholder="Mô tả cụ thể sự việc..."></textarea>
        </div>
        <div style="display: flex; gap: 0.5rem;">
          <button type="button" class="btn-secondary" style="flex: 1;" onclick="app.closeReportModal()">Hủy</button>
          <button type="button" class="btn-primary" style="flex: 1; background-color: var(--accent-red); border-color: var(--accent-red);" onclick="app.submitReport()">Gửi báo cáo</button>
        </div>
      </div>
    `;
    document.body.appendChild(modalDiv);
  }

  openReportModal(reportedUserId = '', reportedPostId = '', reportedCommentId = '') {
    this.injectReportModal();
    this.currentReportTarget = { reportedUserId, reportedPostId, reportedCommentId };

    const heading = document.getElementById('reportModalHeading');
    if (heading) {
      if (reportedUserId) heading.textContent = '🚩 Báo cáo người dùng';
      else if (reportedPostId) heading.textContent = '🚩 Báo cáo bài viết';
      else if (reportedCommentId) heading.textContent = '🚩 Báo cáo bình luận';
      else heading.textContent = '🚩 Báo cáo vi phạm';
    }

    const modal = document.getElementById('reportModal');
    if (modal) {
      const detailInput = document.getElementById('reportDetailInput');
      if (detailInput) detailInput.value = '';
      modal.style.display = 'flex';
    }
  }

  closeReportModal() {
    const modal = document.getElementById('reportModal');
    if (modal) modal.style.display = 'none';
    this.currentReportTarget = null;
  }

  async submitReport() {
    if (!this.currentReportTarget || !this.currentUser) return;
    const reason = document.getElementById('reportReasonSelect').value;
    const detail = document.getElementById('reportDetailInput').value.trim();
    const fullReason = detail ? `${reason}: ${detail}` : reason;

    try {
      const res = await fetch('/api/reports/create', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          reporter_id: this.currentUser.id,
          reported_user_id: this.currentReportTarget.reportedUserId || '',
          reported_post_id: this.currentReportTarget.reportedPostId || '',
          reported_comment_id: this.currentReportTarget.reportedCommentId || '',
          reason: fullReason
        })
      });
      const data = await res.json();
      if (data.status === 'ok') {
        this.showToast('Báo cáo của bạn đã được gửi tới Quản trị viên xử lý!', 'success');
        this.closeReportModal();
      } else {
        this.showToast(data.message || 'Lỗi gửi báo cáo!', 'error');
      }
    } catch (err) {
      this.showToast('Lỗi: ' + err.message, 'error');
    }
  }

  // =========================================================================
  // 15. TRANG QUẢN TRỊ ADMIN (admin.html)
  // =========================================================================
  // Gọi API quản trị: luôn kèm token do server cấp khi Admin đăng nhập
  adminFetch(url, options = {}) {
    const headers = Object.assign({}, options.headers || {}, {
      'X-Admin-Token': localStorage.getItem('cs_admin_token') || ''
    });
    return fetch(url, Object.assign({}, options, { headers })).then(async res => {
      // Phiên quản trị hết hiệu lực (ví dụ đăng nhập từ bản cũ): quay lại màn hình đăng nhập để nhập tài khoản + mật khẩu
      let authFailed = false;
      if (res.status === 403) {
        try { const d = await res.clone().json(); authFailed = !!(d && d.message && d.message.indexOf('Chỉ quản trị viên') === 0); } catch (_) {}
      }
      if (authFailed && !this._adminSessionRedirecting) {
        this._adminSessionRedirecting = true;
        localStorage.removeItem('cs_user');
        localStorage.removeItem('cs_admin_token');
        sessionStorage.setItem('cs_logged_out', '1');
        this.showToast('Phiên quản trị đã hết hạn, vui lòng đăng nhập lại bằng tài khoản và mật khẩu.', 'error');
        setTimeout(() => { window.location.href = 'index.html'; }, 1200);
      }
      return res;
    });
  }

  async loadAdminAuditTable() {
    const tbody = document.getElementById('adminAuditTableBody');
    if (!tbody) return;
    try {
      const res = await this.adminFetch(`/api/admin/audit-logs?admin_id=${encodeURIComponent(this.currentUser.id)}`);
      let data = null;
      try { data = await res.json(); } catch (_) {}
      if (!res.ok || !data || data.status !== 'ok') throw new Error((data && data.message) || `HTTP ${res.status}`);
      const logs = data.logs || [];
      if (!logs.length) {
        tbody.innerHTML = '<tr><td colspan="5" style="text-align:center;color:var(--text-muted);padding:1.5rem;">Chưa có hành động quản trị nào.</td></tr>';
        return;
      }
      tbody.innerHTML = logs.map(l => `
        <tr>
          <td>#${l.id}</td>
          <td>${this.escapeHtml(l.admin_name || l.admin_id || '')}</td>
          <td><strong>${this.escapeHtml(l.action || '')}</strong></td>
          <td>${this.escapeHtml(l.target_type || '')} ${this.escapeHtml(l.target_id || '')}</td>
          <td>${this.escapeHtml(l.detail || '')}<div style="font-size:0.72rem;color:var(--text-muted);">${this.escapeHtml(l.created_at || '')}</div></td>
        </tr>`).join('');
    } catch (e) {
      tbody.innerHTML = this.adminLoadErrorRow(5, 'nhật ký', e);
    }
  }

  async adminClassifyReport(reportId, category) {
    if (!category) return;
    try {
      const res = await this.adminFetch('/api/admin/classify-report', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ admin_id: this.currentUser.id, report_id: reportId, category })
      });
      const data = await res.json();
      this.showToast(data.message || 'Đã phân loại', data.status === 'ok' ? 'success' : 'error');
      if (data.status === 'ok') this.loadAdminReportsTable();
    } catch (e) {}
  }

  initAdminPage() {
    if (!this.currentUser || this.currentUser.role !== 'ADMIN') {
      this.showToast('Chỉ tài khoản Quản trị viên (Admin) mới có quyền truy cập trang này!', 'error');
      setTimeout(() => { window.location.href = 'index.html'; }, 1500);
      return;
    }
    this.loadAdminStats();
    this.switchAdminPageTab('reports');
  }

  switchAdminPageTab(tab) {
    ['tabAdmReportsBtn', 'tabAdmUsersBtn', 'tabAdmPostsBtn', 'tabAdmAuditBtn'].forEach(id => {
      document.getElementById(id)?.classList.remove('active');
    });
    ['admSectionReports', 'admSectionUsers', 'admSectionPosts', 'admSectionAudit'].forEach(id => {
      const el = document.getElementById(id);
      if (el) el.style.display = 'none';
    });

    if (tab === 'reports') {
      document.getElementById('tabAdmReportsBtn')?.classList.add('active');
      document.getElementById('admSectionReports').style.display = 'block';
      this.loadAdminReportsTable(false);
    } else if (tab === 'users') {
      document.getElementById('tabAdmUsersBtn')?.classList.add('active');
      document.getElementById('admSectionUsers').style.display = 'block';
      this.loadAdminUsersTable();
    } else if (tab === 'posts') {
      document.getElementById('tabAdmPostsBtn')?.classList.add('active');
      document.getElementById('admSectionPosts').style.display = 'block';
      this.loadAdminPostsTable();
    } else if (tab === 'audit') {
      document.getElementById('tabAdmAuditBtn')?.classList.add('active');
      document.getElementById('admSectionAudit').style.display = 'block';
      this.loadAdminAuditTable();
    }
  }

  async loadAdminStats() {
    try {
      const res = await this.adminFetch(`/api/admin/stats?admin_id=${encodeURIComponent(this.currentUser.id)}`);
      const data = await res.json();
      if (data.status === 'ok') {
        const s = data.stats;
        document.getElementById('admStatUsers').textContent = s.total_users;
        document.getElementById('admStatLocked').textContent = s.locked_users;
        document.getElementById('admStatPosts').textContent = s.total_posts;
        document.getElementById('admStatMessages').textContent = s.total_messages;
        document.getElementById('admStatPendingReports').textContent = s.pending_reports;
        const ipEl = document.getElementById('admStatInProgress'); if (ipEl) ipEl.textContent = s.in_progress_reports || 0;
        document.getElementById('admStatGroups').textContent = s.total_groups;
      }
    } catch (e) {}
  }

  // ---- Báo cáo vi phạm: tải 15 báo cáo/lần, cuộn tới báo cáo cuối cùng thì tải thêm 15 ----
  // keepCount = true: làm mới nhưng giữ nguyên số báo cáo đang hiển thị (dùng sau khi xử lý/phân loại)
  // keepCount = false: tải lại từ đầu (15 báo cáo đầu tiên)
  async loadAdminReportsTable(keepCount = true) {
    const tbody = document.getElementById('adminReportsTableBody');
    if (!tbody) return;
    const PAGE = this.ADMIN_REPORTS_PAGE_SIZE || 15;
    const loaded = (this.adminReportsCache || []).length;
    const limit = keepCount ? Math.max(PAGE, loaded) : PAGE;
    const seq = (this._admReportsSeq = (this._admReportsSeq || 0) + 1);
    this._admReportsLoading = true;
    this._admReportsHasMore = false;
    this.updateAdminReportsFooter();
    try {
      const data = await this.fetchAdminReportsPage(0, limit);
      if (seq !== this._admReportsSeq) return; // đã có yêu cầu mới hơn (ví dụ đang gõ tìm kiếm)
      this.adminReportsCache = data.reports || [];
      this._admReportsHasMore = !!data.has_more;
      this.renderAdminReportsRows(this.adminReportsCache);
    } catch (e) {
      if (seq !== this._admReportsSeq) return;
      this.adminReportsCache = [];
      tbody.innerHTML = this.adminLoadErrorRow(7, 'báo cáo', e);
    } finally {
      if (seq === this._admReportsSeq) {
        this._admReportsLoading = false;
        this.updateAdminReportsFooter();
        this.setupAdminReportsInfiniteScroll();
      }
    }
  }

  async loadMoreAdminReports() {
    if (this._admReportsLoading || !this._admReportsHasMore) return;
    const section = document.getElementById('admSectionReports');
    if (!section || section.style.display === 'none') return;
    const tbody = document.getElementById('adminReportsTableBody');
    if (!tbody) return;
    const PAGE = this.ADMIN_REPORTS_PAGE_SIZE || 15;
    const seq = this._admReportsSeq;
    const offset = (this.adminReportsCache || []).length;
    this._admReportsLoading = true;
    this.updateAdminReportsFooter();
    try {
      const data = await this.fetchAdminReportsPage(offset, PAGE);
      if (seq !== this._admReportsSeq) return;
      // Bỏ các báo cáo đã có (phòng khi có báo cáo mới chen vào đầu danh sách)
      const have = new Set((this.adminReportsCache || []).map(r => r.id));
      const more = (data.reports || []).filter(r => !have.has(r.id));
      this.adminReportsCache = (this.adminReportsCache || []).concat(more);
      this._admReportsHasMore = !!data.has_more;
      if (more.length) tbody.insertAdjacentHTML('beforeend', more.map(r => this.buildAdminReportRow(r)).join(''));
    } catch (e) {
      if (seq !== this._admReportsSeq) return;
      this._admReportsHasMore = false;
      this.showToast('Không tải thêm được báo cáo: ' + (e.message || e), 'error');
    } finally {
      if (seq === this._admReportsSeq) {
        this._admReportsLoading = false;
        this.updateAdminReportsFooter();
        // Nếu màn hình vẫn còn trống phía dưới thì tải tiếp luôn
        requestAnimationFrame(() => this.checkAdminReportsSentinel());
      }
    }
  }

  async fetchAdminReportsPage(offset, limit) {
    const q = (this._admReportsKeyword || '').trim();
    const url = `/api/admin/reports?admin_id=${encodeURIComponent(this.currentUser.id)}&offset=${offset}&limit=${limit}` +
      (q ? `&q=${encodeURIComponent(q)}` : '');
    const res = await this.adminFetch(url);
    let data = null;
    try { data = await res.json(); } catch (_) {}
    if (!res.ok || !data || data.status !== 'ok') {
      throw new Error((data && data.message) || `HTTP ${res.status}`);
    }
    // Server cũ chưa hỗ trợ phân trang (trả về toàn bộ, không có has_more): tự cắt theo trang
    if (typeof data.has_more === 'undefined') {
      const all = data.reports || [];
      return { reports: all.slice(offset, offset + limit), has_more: all.length > offset + limit };
    }
    return data;
  }

  updateAdminReportsFooter() {
    const el = document.getElementById('adminReportsSentinel');
    if (!el) return;
    const n = (this.adminReportsCache || []).length;
    if (this._admReportsLoading) el.textContent = n ? 'Đang tải thêm báo cáo...' : '';
    else if (this._admReportsHasMore) el.textContent = `Đã hiển thị ${n} báo cáo · cuộn xuống để tải thêm`;
    else el.textContent = n ? `Đã hiển thị tất cả ${n} báo cáo` : '';
  }

  setupAdminReportsInfiniteScroll() {
    const sentinel = document.getElementById('adminReportsSentinel');
    if (!sentinel || this._admReportsObserver) return;
    if ('IntersectionObserver' in window) {
      this._admReportsObserver = new IntersectionObserver(entries => {
        if (entries.some(en => en.isIntersecting)) this.loadMoreAdminReports();
      }, { rootMargin: '0px 0px 120px 0px' });
      this._admReportsObserver.observe(sentinel);
    } else {
      this._admReportsObserver = true;
      window.addEventListener('scroll', () => this.checkAdminReportsSentinel(), { passive: true });
    }
  }

  checkAdminReportsSentinel() {
    const sentinel = document.getElementById('adminReportsSentinel');
    if (!sentinel || sentinel.offsetParent === null) return;
    if (sentinel.getBoundingClientRect().top <= window.innerHeight + 120) this.loadMoreAdminReports();
  }

  // Tìm kiếm báo cáo trên server (vì danh sách chỉ tải dần từng 15 báo cáo)
  filterAdminReportsTable(keyword) {
    this._admReportsKeyword = keyword || '';
    clearTimeout(this._admReportsSearchTimer);
    this._admReportsSearchTimer = setTimeout(() => this.loadAdminReportsTable(false), 300);
  }

  // Dòng báo lỗi thay cho "Đang tải..." khi API không phản hồi đúng
  adminLoadErrorRow(colspan, what, err) {
    const msg = (err && err.message) ? err.message : String(err || '');
    const hint = /HTTP 404/.test(msg)
      ? 'Server đang chạy là bản cũ, chưa có API này. Hãy build lại server.exe (build.bat) rồi chạy lại.'
      : 'Vui lòng thử bấm Làm mới hoặc kiểm tra server.';
    return `<tr><td colspan="${colspan}" style="text-align:center;color:var(--accent-red);padding:1.5rem;">` +
      `Không tải được ${what} (${this.escapeHtml(msg)}).<br><span style="color:var(--text-muted);font-size:0.8rem;">${hint}</span></td></tr>`;
  }

  renderAdminReportsRows(reports) {
    const tbody = document.getElementById('adminReportsTableBody');
    if (!tbody) return;

    if (!reports || reports.length === 0) {
      tbody.innerHTML = '<tr><td colspan="7" style="text-align: center; color: var(--text-muted); padding: 1.5rem;">Không tìm thấy báo cáo vi phạm phù hợp.</td></tr>';
      return;
    }

    tbody.innerHTML = reports.map(r => this.buildAdminReportRow(r)).join('');
  }

  buildAdminReportRow(r) {
    const statusLabel = { PENDING: 'Chờ xử lý', IN_PROGRESS: 'Đang xử lý', RESOLVED: 'Đã chấp nhận', DISMISSED: 'Đã từ chối' };
    const categories = [['SPAM', 'Spam'], ['HARASSMENT', 'Quấy rối'], ['INAPPROPRIATE', 'Nội dung không phù hợp'], ['FAKE_ACCOUNT', 'Tài khoản giả'], ['TECHNICAL', 'Sự cố kỹ thuật'], ['OTHER', 'Khác']];
    {
      let targetText = '--';
      if (r.reported_user_id) targetText = `👤 ${this.escapeHtml(r.reported_user_name)} (ID: ${this.escapeHtml(r.reported_user_id)})`;
      else if (r.reported_post_id) targetText = `📝 Bài viết: ${this.escapeHtml(r.reported_post_id)}` + (r.reported_post_content ? `<div style="font-size:0.78rem;color:var(--text-muted);margin-top:4px;max-width:240px;">${this.escapeHtml(r.reported_post_content)}</div>` : '');
      else if (r.reported_comment_id) targetText = `💬 Bình luận: ${this.escapeHtml(r.reported_comment_id)}`;

      const done = (r.status === 'RESOLVED' || r.status === 'DISMISSED');
      const statusBadge = `<span class="status-badge ${String(r.status).toLowerCase()}">${statusLabel[r.status] || this.escapeHtml(r.status)}</span>`;
      const catSelect = `<select onchange="app.adminClassifyReport(${r.id}, this.value)" style="font-size:0.75rem;margin-top:6px;background:var(--bg-surface-secondary);color:var(--text-main);border:1px solid var(--border-color);border-radius:6px;padding:2px 4px;">
          <option value="">-- Phân loại --</option>
          ${categories.map(c => `<option value="${c[0]}" ${r.category === c[0] ? 'selected' : ''}>${c[1]}</option>`).join('')}
        </select>`;
      const resolvedInfo = done ? `<div style="font-size:0.72rem;color:var(--text-muted);margin-top:4px;">Xử lý lúc ${this.escapeHtml(r.resolved_at || '')}${r.resolution_note ? `<br>Ghi chú: ${this.escapeHtml(r.resolution_note)}` : ''}</div>` : '';

      return `
        <tr>
          <td>#${r.id}</td>
          <td><strong>${this.escapeHtml(r.reporter_name)}</strong></td>
          <td>${targetText}</td>
          <td>${this.escapeHtml(r.reason)}${catSelect}</td>
          <td>${this.formatTimeAgo(r.created_at)}</td>
          <td>${statusBadge}${resolvedInfo}</td>
          <td>
            <div style="display: flex; gap: 0.35rem; flex-wrap: wrap;">
              ${r.reported_user_id ? `<button class="btn-secondary" style="font-size: 0.75rem; color: var(--accent-red);" onclick="app.adminToggleLock(${app.jsArg(r.reported_user_id)})">Khóa user</button>` : ''}
              ${r.reported_post_id ? `<button class="btn-secondary" style="font-size: 0.75rem; color: var(--accent-red);" onclick="app.adminDeletePost(${app.jsArg(r.reported_post_id)})">Xóa bài</button>` : ''}
              ${!done ? `<button class="btn-secondary" style="font-size: 0.75rem;" onclick="app.adminResolveReport(${r.id}, 'IN_PROGRESS')">Đang xử lý</button>
              <button class="btn-primary" style="font-size: 0.75rem;" onclick="app.adminResolveReport(${r.id}, 'RESOLVED')">Chấp nhận</button>
              <button class="btn-secondary" style="font-size: 0.75rem;" onclick="app.adminResolveReport(${r.id}, 'DISMISSED')">Từ chối</button>` : ''}
            </div>
          </td>
        </tr>
      `;
    }
  }

  async loadAdminUsersTable() {
    const tbody = document.getElementById('adminUsersTableBody');
    if (!tbody) return;

    try {
      const res = await this.adminFetch(`/api/admin/users?admin_id=${encodeURIComponent(this.currentUser.id)}`);
      const data = await res.json();
      if (data.status === 'ok') {
        this.adminUsersCache = data.users || [];
        this.renderAdminUsersRows(this.adminUsersCache);
      }
    } catch (e) {}
  }

  renderAdminUsersRows(users) {
    const tbody = document.getElementById('adminUsersTableBody');
    if (!tbody) return;

    tbody.innerHTML = users.map(u => {
      const isLocked = (u.status === 'LOCKED');
      const statusBadge = isLocked ? '<span class="status-badge locked">ĐÃ KHÓA</span>' : '<span class="status-badge active">HOẠT ĐỘNG</span>';
      const actionBtn = isLocked ?
        `<button class="btn-primary" style="font-size: 0.75rem; padding: 0.25rem 0.6rem;" onclick="app.adminToggleLock(${app.jsArg(u.id)})">🔓 Mở khóa</button>` :
        `<button class="btn-secondary" style="font-size: 0.75rem; padding: 0.25rem 0.6rem; color: var(--accent-red);" onclick="app.adminToggleLock(${app.jsArg(u.id)})">🔒 Khóa tài khoản</button>`;

      return `
        <tr>
          <td style="display: flex; align-items: center; gap: 0.5rem;">
            <img src="${app.escapeHtml(u.avatar || '/img/default-avatar.svg')}" style="width: 28px; height: 28px; border-radius: 50%;">
            <strong>${this.escapeHtml(u.fullname)}</strong>
          </td>
          <td>@${this.escapeHtml(u.username)}<br><span style="font-size: 0.75rem; color: var(--text-muted);">${this.escapeHtml(u.email)}</span></td>
          <td>${this.escapeHtml(u.school || u.hometown || '--')}</td>
          <td><span style="font-weight: 700;">${u.role}</span></td>
          <td>${statusBadge}</td>
          <td>${this.formatTimeAgo(u.last_active_at)}</td>
          <td>${u.role !== 'ADMIN' ? `${actionBtn} <button class="btn-secondary" style="font-size:0.75rem;padding:0.25rem 0.6rem;color:var(--accent-red);margin-left:4px;" onclick="app.adminDeleteUser(${app.jsArg(u.id)}, ${app.jsArg(u.fullname || '')})">🗑️ Xóa</button>` : '<span style="color: var(--text-muted); font-size: 0.75rem;">Quản trị</span>'}</td>
        </tr>
      `;
    }).join('');
  }

  filterAdminUsersTable(keyword) {
    if (!this.adminUsersCache) return;
    const q = keyword.toLowerCase().trim();
    if (!q) {
      this.renderAdminUsersRows(this.adminUsersCache);
      return;
    }
    const filtered = this.adminUsersCache.filter(u =>
      u.fullname.toLowerCase().includes(q) ||
      u.username.toLowerCase().includes(q) ||
      u.email.toLowerCase().includes(q)
    );
    this.renderAdminUsersRows(filtered);
  }

  async loadAdminPostsTable() {
    const tbody = document.getElementById('adminPostsTableBody');
    if (!tbody) return;

    try {
      const res = await this.adminFetch(`/api/admin/posts?admin_id=${encodeURIComponent(this.currentUser.id)}&limit=100`);
      let data = null;
      try { data = await res.json(); } catch (_) {}
      if (!res.ok || !data || data.status !== 'ok') throw new Error((data && data.message) || `HTTP ${res.status}`);
      this.adminPostsCache = data.posts || [];
      const kw = (document.getElementById('admSearchPostInput') || {}).value || '';
      if (kw.trim()) this.filterAdminPostsTable(kw); else this.renderAdminPostsRows(this.adminPostsCache);
    } catch (e) {
      tbody.innerHTML = this.adminLoadErrorRow(6, 'bài viết', e);
    }
  }

  filterAdminPostsTable(keyword) {
    if (!this.adminPostsCache) return;
    const q = keyword.toLowerCase().trim();
    if (!q) {
      this.renderAdminPostsRows(this.adminPostsCache);
      return;
    }
    const filtered = this.adminPostsCache.filter(p =>
      (p.author_name && p.author_name.toLowerCase().includes(q)) ||
      (p.content && p.content.toLowerCase().includes(q)) ||
      (p.title && p.title.toLowerCase().includes(q)) ||
      (p.id && p.id.toLowerCase().includes(q))
    );
    this.renderAdminPostsRows(filtered);
  }

  renderAdminPostsRows(posts) {
    const tbody = document.getElementById('adminPostsTableBody');
    if (!tbody) return;

    if (!posts || posts.length === 0) {
      tbody.innerHTML = '<tr><td colspan="6" style="text-align: center; color: var(--text-muted); padding: 1.5rem;">Không tìm thấy bài viết phù hợp.</td></tr>';
      return;
    }

    tbody.innerHTML = posts.map(p => `
      <tr>
        <td><code>${this.escapeHtml(p.id)}</code></td>
        <td><strong>${this.escapeHtml(p.author_name || '')}</strong></td>
        <td style="max-width: 300px; white-space: nowrap; overflow: hidden; text-overflow: ellipsis;">
          ${p.title ? `<strong>${this.escapeHtml(p.title)}:</strong> ` : ''}${this.escapeHtml(p.content || '')}
        </td>
        <td>${this.escapeHtml(p.media_type || 'NONE')}</td>
        <td>${this.formatTimeAgo(p.created_at)}</td>
        <td>
          <button class="btn-secondary" style="font-size: 0.75rem; color: var(--accent-red);" onclick="app.adminDeletePost(${app.jsArg(p.id)})">🗑️ Xóa bài</button>
        </td>
      </tr>
    `).join('');
  }

  async adminToggleLock(userId) {
    try {
      const res = await this.adminFetch('/api/admin/toggle-lock', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ admin_id: this.currentUser.id, user_id: userId })
      });
      const data = await res.json();
      if (data.status === 'ok') {
        this.showToast(data.message, 'success');
        this.loadAdminStats();
        if (window.location.pathname.includes('admin.html')) {
          this.loadAdminUsersTable();
          this.loadAdminReportsTable();
        }
      }
    } catch (e) {}
  }

  async adminResolveReport(reportId, action, note = '') {
    try {
      if (!note && action !== 'IN_PROGRESS') {
        note = window.prompt(action === 'RESOLVED'
          ? 'Ghi chú xử lý khiếu nại (sẽ gửi cho người báo cáo):'
          : 'Lý do từ chối / bỏ qua khiếu nại:') || '';
      }
      const res = await this.adminFetch('/api/admin/resolve-report', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ admin_id: this.currentUser.id, report_id: reportId, action, note: note || '' })
      });
      const data = await res.json();
      if (data.status === 'ok') {
        this.showToast('Đã xử lý báo cáo!', 'success');
        this.loadAdminStats();
        this.loadAdminReportsTable();
      }
    } catch (e) {}
  }

  async adminDeletePost(postId) {
    const _c8 = await this.showConfirm('Xóa bài viết vi phạm này?'); if (!_c8) return;
    try {
      const res = await this.adminFetch('/api/admin/delete-post', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ admin_id: this.currentUser.id, post_id: postId })
      });
      const data = await res.json();
      if (data.status === 'ok') {
        this.showToast('Đã xóa bài viết vi phạm!', 'success');
        this.loadAdminStats();
        if (window.location.pathname.includes('admin.html')) {
          this.loadAdminPostsTable();
          this.loadAdminReportsTable();
        } else {
          this.loadFeed();
        }
      }
    } catch (e) {}
  }

  async adminDeleteUser(userId, userName = '') {
    if (!this.currentUser || this.currentUser.role !== 'ADMIN') return;
    const ok = await this.showConfirm(`Xóa tài khoản "${userName || userId}"? Hành động này ẩn danh hóa tài khoản vi phạm.`);
    if (!ok) return;
    try {
      const res = await this.adminFetch('/api/admin/delete-user', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ admin_id: this.currentUser.id, user_id: userId })
      });
      const data = await res.json();
      if (data.status === 'ok') {
        this.showToast(data.message || 'Đã xóa tài khoản.', 'success');
        this.loadAdminUsersTable();
        this.loadAdminStats();
      } else {
        this.showToast(data.message || 'Không thể xóa tài khoản.', 'error');
      }
    } catch (e) {
      this.showToast('Lỗi: ' + e.message, 'error');
    }
  }

  // =========================================================================
  // 16. CHỈNH SỬA HỒ SƠ & TẢI AVATAR
  // =========================================================================
  openEditProfileModal() {
    if (!this.currentUser) return;
    const u = this.currentUser;

    const fn = document.getElementById('editFullname') || document.getElementById('profFullname');
    if (fn) fn.value = u.fullname || '';

    const bio = document.getElementById('editBio') || document.getElementById('profBio');
    if (bio) bio.value = u.bio || '';

    // Populate province options for Hometown and Residence
    const provinces = (typeof VIETNAM_DATA !== 'undefined' && Array.isArray(VIETNAM_DATA.provinces)) ? VIETNAM_DATA.provinces : [
      "Hà Nội", "TP. Hồ Chí Minh", "Đà Nẵng", "Hải Phòng", "Cần Thơ", "An Giang", "Bà Rịa - Vũng Tàu",
      "Bắc Giang", "Bắc Kạn", "Bạc Liêu", "Bắc Ninh", "Bến Tre", "Bình Định", "Bình Dương", "Bình Phước",
      "Bình Thuận", "Cà Mau", "Cao Bằng", "Đắk Lắk", "Đắk Nông", "Điện Biên", "Đồng Nai", "Đồng Tháp",
      "Gia Lai", "Hà Giang", "Hà Nam", "Hà Tĩnh", "Hải Dương", "Hậu Giang", "Hòa Bình", "Hưng Yên",
      "Khánh Hòa", "Kiên Giang", "Kon Tum", "Lai Châu", "Lâm Đồng", "Lạng Sơn", "Lào Cai", "Long An",
      "Nam Định", "Nghệ An", "Ninh Bình", "Ninh Thuận", "Phú Thọ", "Phú Yên", "Quảng Bình", "Quảng Nam",
      "Quảng Ngãi", "Quảng Ninh", "Quảng Trị", "Sóc Trăng", "Sơn La", "Tây Ninh", "Thái Bình", "Thái Nguyên",
      "Thanh Hóa", "Thừa Thiên Huế", "Tiền Giang", "Trà Vinh", "Tuyên Quang", "Vĩnh Long", "Vĩnh Phúc", "Yên Bái"
    ];

    const populateSelect = (elId, selectedVal) => {
      const sel = document.getElementById(elId);
      if (!sel) return;
      if (sel.tagName === 'SELECT') {
        if (sel.options.length <= 1) {
          provinces.forEach(p => {
            const opt = document.createElement('option');
            opt.value = p;
            opt.textContent = p;
            sel.appendChild(opt);
          });
        }
        sel.value = selectedVal || '';
      } else {
        sel.value = selectedVal || '';
      }
    };

    populateSelect('profHometownSelect', u.hometown);
    populateSelect('editHometown', u.hometown);
    populateSelect('profResidenceSelect', u.residence);
    populateSelect('editResidence', u.residence);

    const gen = document.getElementById('profGenderSelect') || document.getElementById('editGender');
    if (gen) gen.value = u.gender || '';

    const sc = document.getElementById('editSchool') || document.getElementById('profSchoolInput');
    if (sc) sc.value = u.school || '';

    const wp = document.getElementById('editWorkplace') || document.getElementById('profWorkplace');
    if (wp) wp.value = u.workplace || '';

    const it = document.getElementById('editInterests') || document.getElementById('profInterestsInput');
    if (it) it.value = (u.interests && u.interests !== '--') ? u.interests.split(/[;,]/).map(s => s.trim()).filter(Boolean).join(', ') : '';

    this.initVietnamDataOptions();

    const modal = document.getElementById('editProfileModal');
    if (modal) modal.style.display = 'flex';
  }

  closeEditProfileModal() {
    const modal = document.getElementById('editProfileModal');
    if (modal) modal.style.display = 'none';
  }

  saveProfile() {
    this.saveProfileChanges();
  }

  // --- ĐỔI MẬT KHẨU ---
  openChangePasswordModal() {
    // Đảm bảo modal tồn tại (inject nếu chưa có)
    if (!document.getElementById('changePasswordModal')) {
      const div = document.createElement('div');
      div.innerHTML = `
        <div class="modal-overlay" id="changePasswordModal" style="display:none;z-index:2050;" onclick="if(event.target===this)app.closeChangePasswordModal()">
          <div class="auth-card" style="max-width:420px;width:92%;">
            <div class="auth-header" style="display:flex;justify-content:space-between;align-items:center;margin-bottom:1rem;">
              <h2 style="font-size:1.25rem;font-weight:700;margin:0;">🔒 Đổi mật khẩu</h2>
              <button type="button" onclick="app.closeChangePasswordModal()" style="background:none;border:none;font-size:1.25rem;cursor:pointer;color:var(--text-muted);">✖</button>
            </div>
            <!-- Khung cảnh báo lỗi trực quan phía trên form -->
            <div id="changePasswordErrorAlert" style="display:none; color: #dc2626; background: #fee2e2; border: 1px solid #fca5a5; padding: 0.6rem 0.8rem; border-radius: 8px; font-size: 0.85rem; margin-bottom: 0.85rem; font-weight: 500;"></div>
            <div class="form-group" style="margin-bottom:0.85rem;">
              <label style="font-weight:600;font-size:0.85rem;margin-bottom:0.35rem;display:block;">Mật khẩu cũ:</label>
              <input type="password" id="oldPasswordInput" placeholder="Nhập mật khẩu hiện tại..." autocomplete="current-password" style="width:100%;padding:0.6rem 0.8rem;border-radius:8px;border:1px solid var(--border-color);background:var(--bg-surface-secondary);color:var(--text-primary);font-size:0.9rem;">
            </div>
            <div class="form-group" style="margin-bottom:0.85rem;">
              <label style="font-weight:600;font-size:0.85rem;margin-bottom:0.35rem;display:block;">Mật khẩu mới:</label>
              <input type="password" id="newPasswordInput" placeholder="Nhập mật khẩu mới (tối thiểu 6 ký tự)..." autocomplete="new-password" style="width:100%;padding:0.6rem 0.8rem;border-radius:8px;border:1px solid var(--border-color);background:var(--bg-surface-secondary);color:var(--text-primary);font-size:0.9rem;">
            </div>
            <div class="form-group" style="margin-bottom:1.2rem;">
              <label style="font-weight:600;font-size:0.85rem;margin-bottom:0.35rem;display:block;">Xác nhận mật khẩu mới:</label>
              <input type="password" id="confirmNewPasswordInput" placeholder="Nhập lại mật khẩu mới..." autocomplete="new-password" style="width:100%;padding:0.6rem 0.8rem;border-radius:8px;border:1px solid var(--border-color);background:var(--bg-surface-secondary);color:var(--text-primary);font-size:0.9rem;">
            </div>
            <div style="display:flex;gap:0.75rem;">
              <button type="button" class="btn-secondary" style="flex:1;" onclick="app.closeChangePasswordModal()">Hủy</button>
              <button type="button" class="btn-primary" style="flex:1;" onclick="app.submitChangePassword()">Xác nhận</button>
            </div>
          </div>
        </div>`;
      document.body.appendChild(div.firstElementChild);
    }
    // Xóa sạch input và thông báo lỗi cũ
    ['oldPasswordInput','newPasswordInput','confirmNewPasswordInput'].forEach(id => {
      const el = document.getElementById(id);
      if (el) el.value = '';
    });
    const errAlert = document.getElementById('changePasswordErrorAlert');
    if (errAlert) {
      errAlert.style.display = 'none';
      errAlert.textContent = '';
    }

    // Đóng modal chỉnh sửa hồ sơ trước khi mở modal đổi mật khẩu
    const editModal = document.getElementById('editProfileModal');
    if (editModal) editModal.style.display = 'none';
    const modal = document.getElementById('changePasswordModal');
    if (modal) modal.style.display = 'flex';
    // Focus ô mật khẩu cũ
    setTimeout(() => {
      const inp = document.getElementById('oldPasswordInput');
      if (inp) inp.focus();
    }, 100);
  }

  closeChangePasswordModal() {
    const modal = document.getElementById('changePasswordModal');
    if (modal) modal.style.display = 'none';
    const errAlert = document.getElementById('changePasswordErrorAlert');
    if (errAlert) {
      errAlert.style.display = 'none';
      errAlert.textContent = '';
    }
  }

  async submitChangePassword() {
    const errAlert = document.getElementById('changePasswordErrorAlert');
    const showError = (msg) => {
      if (errAlert) {
        errAlert.textContent = '⚠️ ' + msg;
        errAlert.style.display = 'block';
      }
      this.showToast(msg, 'error');
    };

    if (errAlert) {
      errAlert.style.display = 'none';
      errAlert.textContent = '';
    }

    const oldPass = (document.getElementById('oldPasswordInput')?.value || '').trim();
    const newPass = (document.getElementById('newPasswordInput')?.value || '').trim();
    const confirmPass = (document.getElementById('confirmNewPasswordInput')?.value || '').trim();

    if (!oldPass || !newPass || !confirmPass) {
      showError('Vui lòng điền đầy đủ 3 trường thông tin!');
      return;
    }
    if (newPass.length < 6) {
      showError('Mật khẩu mới phải có ít nhất 6 ký tự!');
      return;
    }
    if (newPass !== confirmPass) {
      showError('Mật khẩu mới và xác nhận mật khẩu không khớp!');
      return;
    }

    try {
      const res = await fetch('/api/users/change-password', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          user_id: this.currentUser.id,
          old_password: oldPass,
          new_password: newPass,
          confirm_password: confirmPass
        })
      });
      const data = await res.json();
      if (data.status === 'ok') {
        this.showToast('Đổi mật khẩu thành công! 🎉', 'success');
        this.closeChangePasswordModal();
      } else {
        showError(data.message || 'Đổi mật khẩu thất bại!');
      }
    } catch (e) {
      showError('Lỗi kết nối: ' + e.message);
    }
  }

  async saveProfileChanges() {
    const fn = document.getElementById('editFullname') || document.getElementById('profFullname');
    const bio = document.getElementById('editBio') || document.getElementById('profBio');
    const ht = document.getElementById('editHometown') || document.getElementById('profHometownSelect');
    const resEl = document.getElementById('editResidence') || document.getElementById('profResidenceSelect');
    const gen = document.getElementById('editGender') || document.getElementById('profGenderSelect');
    const sc = document.getElementById('editSchool') || document.getElementById('profSchoolInput');
    const wp = document.getElementById('editWorkplace') || document.getElementById('profWorkplace');
    const it = document.getElementById('editInterests') || document.getElementById('profInterestsInput');

    const payload = {
      id: this.currentUser.id,
      fullname: fn ? fn.value.trim() : '',
      bio: bio ? bio.value.trim() : '',
      hometown: ht ? ht.value.trim() : '',
      residence: resEl ? resEl.value.trim() : '',
      gender: gen ? gen.value.trim() : '',
      school: sc ? sc.value.trim() : '',
      workplace: wp ? wp.value.trim() : '',
      interests: it ? it.value.trim() : ''
    };

    try {
      const res = await fetch('/api/users/update', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
      });
      const data = await res.json();
      if (data.status === 'ok') {
        this.showToast('Cập nhật hồ sơ thành công!', 'success');
        this.currentUser = data.user;
        localStorage.setItem('cs_user', JSON.stringify(this.currentUser));
        this.renderUserUI();
        this.closeEditProfileModal();
        if (window.location.pathname.includes('profile.html')) {
          this.initProfilePage(this.currentUser.id);
        }
      }
    } catch (err) {
      this.showToast('Lỗi: ' + err.message, 'error');
    }
  }

  // --- QUẢN LÝ ẢNH ĐẠI DIỆN (AVATAR) ---
  openAvatarUploadModal() {
    if (!this.currentUser) return;
    const preview = document.getElementById('avatarPreviewImg');
    if (preview) {
      preview.src = this.currentUser.avatar || '/img/default-avatar.svg';
    }
    const modal = document.getElementById('avatarModal');
    if (modal) modal.style.display = 'flex';
  }

  closeAvatarModal() {
    const modal = document.getElementById('avatarModal');
    if (modal) modal.style.display = 'none';
    const fileInput = document.getElementById('avatarFileInput');
    if (fileInput) fileInput.value = '';
  }

  handleAvatarFileChange(input) {
    if (!input || !input.files || !input.files[0]) return;
    const file = input.files[0];
    if (!file.type.startsWith('image/')) {
      this.showToast('Vui lòng chọn tệp hình ảnh hợp lệ!', 'error');
      return;
    }
    const reader = new FileReader();
    reader.onload = async (e) => {
      const dataUrl = e.target.result;
      const preview = document.getElementById('avatarPreviewImg');
      if (preview) preview.src = dataUrl;
      await this.saveNewAvatar(dataUrl);
    };
    reader.readAsDataURL(file);
  }

  async setDefaultAvatar() {
    await this.saveNewAvatar('/img/default-avatar.svg');
  }

  async saveNewAvatar(avatarUrl) {
    if (!this.currentUser) return;
    try {
      const res = await fetch('/api/users/update', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          id: this.currentUser.id,
          avatar: avatarUrl
        })
      });
      const data = await res.json();
      if (data.status === 'ok') {
        this.currentUser.avatar = avatarUrl;
        localStorage.setItem('cs_user', JSON.stringify(this.currentUser));
        this.renderUserUI();
        this.closeAvatarModal();
        this.showToast('Cập nhật ảnh đại diện thành công!', 'success');
        if (window.location.pathname.includes('profile.html')) {
          this.initProfilePage(this.currentUser.id);
        }
      } else {
        this.showToast(data.message || 'Lỗi cập nhật ảnh đại diện!', 'error');
      }
    } catch (e) {
      this.showToast('Lỗi: ' + e.message, 'error');
    }
  }



  // =========================================================================
  // 16B. MODAL CHỌN SỞ THÍCH (CHO PHÉP NHIỀU, TÌM KIẾM, DẤU ✓ / ✕)
  // =========================================================================
  openHobbyPickerModal(targetInputId) {
    this.hobbyTargetInputId = targetInputId;
    const inputEl = document.getElementById(targetInputId);
    this.selectedHobbies = [];
    if (inputEl && inputEl.value) {
      // Hiển thị đầy đủ các sở thích đã lưu, không cắt còn 3
      this.selectedHobbies = inputEl.value
        .split(/[;,]/)
        .map(s => s.trim())
        .filter(s => s.length > 0);
    }
    const searchInput = document.getElementById('hobbySearchInput');
    if (searchInput) searchInput.value = '';

    const modal = document.getElementById('hobbyPickerModal');
    if (modal) modal.style.display = 'flex';
    this.renderHobbyModal('');
  }

  closeHobbyPickerModal() {
    const modal = document.getElementById('hobbyPickerModal');
    if (modal) modal.style.display = 'none';
  }

  filterHobbyList(keyword) {
    this.renderHobbyModal(keyword);
  }

  toggleHobby(hobby) {
    const idx = this.selectedHobbies.indexOf(hobby);
    if (idx >= 0) {
      this.selectedHobbies.splice(idx, 1);
    } else {
      // Cho phép chọn nhiều sở thích, không giới hạn tối đa 3
      this.selectedHobbies.push(hobby);
    }
    const searchInput = document.getElementById('hobbySearchInput');
    this.renderHobbyModal(searchInput ? searchInput.value : '');
  }

  removeHobbyTag(hobby) {
    const idx = this.selectedHobbies.indexOf(hobby);
    if (idx >= 0) {
      this.selectedHobbies.splice(idx, 1);
      const searchInput = document.getElementById('hobbySearchInput');
      this.renderHobbyModal(searchInput ? searchInput.value : '');
    }
  }

  renderHobbyModal(keyword = '') {
    const tagsBox = document.getElementById('hobbySelectedTagsBox');
    const listContainer = document.getElementById('hobbyListContainer');
    if (!listContainer) return;

    if (tagsBox) {
      if (this.selectedHobbies.length === 0) {
        tagsBox.innerHTML = '<span style="font-size: 0.8rem; color: var(--text-muted); font-style: italic;">Chưa chọn sở thích nào</span>';
      } else {
        tagsBox.innerHTML = this.selectedHobbies.map(h => `
          <div class="hobby-tag-item">
            <span>${this.escapeHtml(h)}</span>
            <span class="remove-tag" onclick="app.removeHobbyTag('${this.escapeHtml(h).replace(/'/g, "\\'")}')">✕</span>
          </div>
        `).join('');
      }
    }

    const q = (keyword || '').toLowerCase().trim();
    const allHobbies = (typeof VIETNAM_DATA !== 'undefined' && Array.isArray(VIETNAM_DATA.hobbies)) ? VIETNAM_DATA.hobbies : [
      "Đọc sách", "Nghe nhạc", "Lập trình", "Bóng đá", "Du lịch", "Nấu ăn", "Nhiếp ảnh", "Chơi game", "Xem phim", "Hội họa",
      "Viết lách", "Cầu lông", "Bơi lội", "Chạy bộ", "Đạp xe", "Thiền & Yoga", "Làm vườn", "Nuôi thú cưng", "Pha chế cà phê"
    ];

    const filtered = allHobbies.filter(h => !q || h.toLowerCase().includes(q));

    if (filtered.length === 0) {
      listContainer.innerHTML = '<div style="text-align: center; color: var(--text-muted); padding: 1.5rem; font-size: 0.85rem;">Không tìm thấy sở thích phù hợp</div>';
      return;
    }

    listContainer.innerHTML = filtered.map(h => {
      const isSelected = this.selectedHobbies.includes(h);
      const icon = isSelected ? '✓' : '✕';
      return `
        <div class="hobby-list-item ${isSelected ? 'selected' : ''}" onclick="app.toggleHobby('${this.escapeHtml(h).replace(/'/g, "\\'")}')">
          <span class="hobby-item-name">${this.escapeHtml(h)}</span>
          <span class="hobby-indicator-icon">${icon}</span>
        </div>
      `;
    }).join('');
  }

  confirmHobbySelection() {
    if (this.hobbyTargetInputId) {
      const inputEl = document.getElementById(this.hobbyTargetInputId);
      if (inputEl) {
        inputEl.value = this.selectedHobbies.join(', ');
      }
    }
    this.closeHobbyPickerModal();
  }

  // =========================================================================
  // 17. ĐĂNG KÝ, ĐĂNG NHẬP, GOOGLE OAUTH & GMAIL OTP
  // =========================================================================
  switchAuthTab(tab) {
    document.getElementById('tabLoginBtn')?.classList.toggle('active', tab === 'login');
    document.getElementById('tabRegisterBtn')?.classList.toggle('active', tab === 'register');
    document.getElementById('loginFormContainer').style.display = (tab === 'login') ? 'block' : 'none';
    document.getElementById('registerStep1Container').style.display = (tab === 'register') ? 'block' : 'none';
    document.getElementById('otpVerificationContainer').style.display = 'none';
  }

  async handleLogin() {
    const id = document.getElementById('loginIdentifier').value.trim();
    const pass = document.getElementById('loginPassword').value;

    if (!id || !pass) {
      this.showToast('Vui lòng nhập tài khoản/email và mật khẩu!', 'error');
      return;
    }

    try {
      const res = await fetch('/api/auth/login', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ identifier: id, password: pass })
      });
      const data = await res.json();
      if (data.status === 'ok') {
        this.currentUser = data.user;
        localStorage.setItem('cs_user', JSON.stringify(this.currentUser));
        if (data.admin_token) localStorage.setItem('cs_admin_token', data.admin_token);
        else localStorage.removeItem('cs_admin_token');
        this.showToast('Đăng nhập thành công!', 'success');
        window.location.reload();
      } else {
        this.showToast(data.message || 'Đăng nhập thất bại!', 'error');
      }
    } catch (err) {
      this.showToast('Lỗi: ' + err.message, 'error');
    }
  }

  async handleRegisterStep1() {
    const fn = document.getElementById('regFullname').value.trim();
    const un = document.getElementById('regUsername').value.trim();
    const em = document.getElementById('regEmail').value.trim();
    const pw = document.getElementById('regPassword').value;
    const cpw = document.getElementById('regConfirmPassword').value;

    if (!fn || !un || !em || !pw || !cpw) {
      this.showToast('Vui lòng điền đầy đủ tất cả các trường!', 'error');
      return;
    }

    if (pw !== cpw) {
      this.showToast('Xác nhận mật khẩu không khớp!', 'error');
      return;
    }

    this.tempRegisterData = { fullname: fn, username: un, email: em, password: pw };

    try {
      const res = await fetch('/api/auth/send-otp', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ email: em, purpose: 'REGISTER' })
      });
      const data = await res.json();
      if (data.status === 'ok') {
        this.showToast(data.message, 'success');
        document.getElementById('registerStep1Container').style.display = 'none';
        document.getElementById('otpVerificationContainer').style.display = 'block';
        document.getElementById('otpTargetEmail').textContent = em;

        // Bắt đầu đếm ngược OTP
        this.startOtpTimer(300);
      } else {
        this.showToast(data.message || 'Không thể gửi mã xác nhận!', 'error');
      }
    } catch (err) {
      this.showToast('Lỗi: ' + err.message, 'error');
    }
  }

  startOtpTimer(seconds) {
    if (this.otpTimerInterval) clearInterval(this.otpTimerInterval);
    let left = seconds;
    const timerSpan = document.getElementById('otpTimer');
    this.otpTimerInterval = setInterval(() => {
      left--;
      if (timerSpan) {
        const m = Math.floor(left / 60);
        const s = left % 60;
        timerSpan.textContent = `${m}:${s < 10 ? '0' : ''}${s}`;
      }
      if (left <= 0) {
        clearInterval(this.otpTimerInterval);
      }
    }, 1000);
  }

  async handleVerifyOtpAndRegister() {
    const b1 = document.getElementById('otp1').value;
    const b2 = document.getElementById('otp2').value;
    const b3 = document.getElementById('otp3').value;
    const b4 = document.getElementById('otp4').value;
    const b5 = document.getElementById('otp5').value;
    const b6 = document.getElementById('otp6').value;
    const code = `${b1}${b2}${b3}${b4}${b5}${b6}`.trim();

    if (code.length !== 6) {
      this.showToast('Vui lòng nhập đủ 6 chữ số mã xác thực!', 'error');
      return;
    }

    try {
      const res = await fetch('/api/auth/register-with-otp', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          ...this.tempRegisterData,
          code: code
        })
      });
      const data = await res.json();
      if (data.status === 'ok') {
        this.currentUser = data.user;
        localStorage.setItem('cs_user', JSON.stringify(this.currentUser));
        this.showToast('Đăng ký tài khoản thành công!', 'success');
        window.location.reload();
      } else {
        this.showToast(data.message || 'Mã xác thực không đúng!', 'error');
      }
    } catch (err) {
      this.showToast('Lỗi: ' + err.message, 'error');
    }
  }

  openGoogleAuthModal() {
    const modal = document.getElementById('googleAuthModal');
    if (modal) modal.style.display = 'flex';
    this.triggerRealGoogleSignIn();
  }

  async triggerRealGoogleSignIn() {
    const button = document.getElementById('googleSignInButton');
    if (!button) return;
    button.innerHTML = '<span style="color:#5f6368;">Đang kết nối Google...</span>';

    try {
      const r = await fetch('/api/auth/config', { cache: 'no-store' });
      if (!r.ok) throw new Error('Không lấy được cấu hình đăng nhập Google từ máy chủ.');
      const d = await r.json();
      const clientId = (d && d.google_client_id) || '';
      if (!clientId) {
        button.innerHTML = '<span style="color:#b3261e;">Máy chủ chưa cấu hình Google Client ID.</span>';
        this.showToast('Đăng nhập Google chưa được bật trên máy chủ.', 'error');
        return;
      }

      if (!(window.google && window.google.accounts && window.google.accounts.id)) {
        const script = document.querySelector('script[src="https://accounts.google.com/gsi/client"]');
        if (!script) throw new Error('Không tìm thấy Google Identity Services trên trang.');
        await new Promise((resolve, reject) => {
          const timeout = setTimeout(() => reject(new Error('Tải Google Identity Services quá thời gian.')), 10000);
          script.addEventListener('load', () => {
            clearTimeout(timeout);
            resolve();
          }, { once: true });
          script.addEventListener('error', () => {
            clearTimeout(timeout);
            reject(new Error('Trình duyệt không tải được Google Identity Services.'));
          }, { once: true });
        });
      }

      if (!(window.google && window.google.accounts && window.google.accounts.id)) {
        throw new Error('Google Identity Services chưa sẵn sàng.');
      }

      window.google.accounts.id.initialize({
        client_id: clientId,
        callback: (response) => this.handleGoogleCredentialResponse(response),
        ux_mode: 'popup'
      });

      button.innerHTML = '';
      window.google.accounts.id.renderButton(button, {
        type: 'standard',
        theme: 'outline',
        size: 'large',
        text: 'continue_with',
        shape: 'pill',
        logo_alignment: 'left',
        width: Math.min(320, button.clientWidth || 320)
      });
    } catch (error) {
      console.error('Không thể khởi tạo đăng nhập Google:', error);
      button.innerHTML = '<span style="color:#b3261e;">Không tải được đăng nhập Google. <button type="button" onclick="app.triggerRealGoogleSignIn()">Thử lại</button></span>';
      this.showToast(error.message || 'Không thể kết nối dịch vụ Google.', 'error');
    }
  }

  async handleGoogleCredentialResponse(response) {
    if (!response || !response.credential) {
      this.showToast('Google không trả về thông tin xác thực. Vui lòng thử lại.', 'error');
      return;
    }
    try {
      const res = await fetch('/api/auth/google-login', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ credential: response.credential })
      });
      const data = await res.json();
      if (data.status === 'ok') {
        this.currentUser = data.user;
        localStorage.setItem('cs_user', JSON.stringify(this.currentUser));
        this.showToast('Đăng nhập tài khoản Google thành công!', 'success');
        window.location.reload();
      } else {
        this.showToast(data.message || 'Lỗi đăng nhập Google!', 'error');
      }
    } catch (error) {
      console.error('Đăng nhập Google thất bại:', error);
      this.showToast('Không thể gửi thông tin xác thực đến máy chủ.', 'error');
    }
  }

  closeGoogleAuthModal() {
    const modal = document.getElementById('googleAuthModal');
    if (modal) modal.style.display = 'none';
  }

  // =========================================================================
  // 18. TOAST NOTIFICATION & ESCAPE HTML
  // =========================================================================
  showToast(message, type = 'info') {
    const container = document.getElementById('toastContainer');
    if (!container) return;

    const toast = document.createElement('div');
    toast.className = `toast ${type}`;
    let icon = 'ℹ️';
    if (type === 'success') icon = '✅';
    if (type === 'error') icon = '⚠️';

    toast.innerHTML = `<span>${icon}</span><span>${this.escapeHtml(String(message == null ? '' : message))}</span>`;
    container.appendChild(toast);

    setTimeout(() => {
      toast.style.opacity = '0';
      toast.style.transform = 'scale(0.85)';
      toast.style.transition = 'all 0.3s ease';
      setTimeout(() => toast.remove(), 300);
    }, 2500);
  }

  // Chuỗi JS an toàn khi nhúng trong onclick="...": JSON.stringify rồi escape HTML (không thể thoát khỏi chuỗi)
  jsArg(v) {
    return this.escapeHtml(JSON.stringify(String(v == null ? '' : v)));
  }

  escapeHtml(str) {
    if (!str) return '';
    return str
      .replace(/&/g, "&amp;")
      .replace(/</g, "&lt;")
      .replace(/>/g, "&gt;")
      .replace(/"/g, "&quot;")
      .replace(/'/g, "&#039;");
  }

  // =========================================================================
  // CUSTOM CONFIRM DIALOG (thay thế window.confirm)
  // =========================================================================
  showConfirm(message, confirmText = 'Xác nhận', cancelText = 'Hủy', isDanger = false) {
    return new Promise(resolve => {
      // Xóa dialog cũ nếu có
      const existing = document.getElementById('customConfirmOverlay');
      if (existing) existing.remove();

      const overlay = document.createElement('div');
      overlay.id = 'customConfirmOverlay';
      overlay.style.cssText = `
        position:fixed;inset:0;background:rgba(0,0,0,0.5);z-index:100001;
        display:flex;align-items:center;justify-content:center;
      `;
      const confirmBg = isDanger ? 'var(--accent-red)' : 'var(--primary)';
      overlay.innerHTML = `
        <div style="background:var(--bg-surface);border-radius:14px;padding:1.5rem 2rem;
          max-width:360px;width:90%;box-shadow:0 8px 32px rgba(0,0,0,0.35);text-align:center;">
          <div style="font-size:1.02rem;font-weight:600;color:var(--text-primary);margin-bottom:1.2rem;line-height:1.4;">
            ${this.escapeHtml(String(message == null ? '' : message))}
          </div>
          <div style="display:flex;gap:0.75rem;justify-content:center;">
            <button id="confirmNo" style="flex:1;padding:0.6rem 1rem;border-radius:8px;
              border:1px solid var(--border-color);background:var(--bg-surface-secondary);
              color:var(--text-muted);cursor:pointer;font-size:0.9rem;font-weight:500;">${cancelText}</button>
            <button id="confirmYes" style="flex:1;padding:0.6rem 1rem;border-radius:8px;
              border:none;background:${confirmBg};color:#fff;cursor:pointer;
              font-size:0.9rem;font-weight:600;">${confirmText}</button>
          </div>
        </div>
      `;
      document.body.appendChild(overlay);
      overlay.querySelector('#confirmYes').onclick = () => { overlay.remove(); resolve(true); };
      overlay.querySelector('#confirmNo').onclick = () => { overlay.remove(); resolve(false); };
      overlay.onclick = (e) => { if (e.target === overlay) { overlay.remove(); resolve(false); } };
    });
  }

  // =========================================================================
  // XÓA KẾT BẠN (HỦY BẠN BÈ TRÊN TRANG PROFILE)
  // =========================================================================
  async handleUnfriend(targetId, targetName) {
    if (!this.currentUser) return;
    const ok = await this.showConfirm(
      `Bạn có chắc chắn muốn hủy kết bạn với <strong>${this.escapeHtml(targetName)}</strong> không?`,
      'Xóa kết bạn',
      'Hủy',
      true
    );
    if (!ok) return;

    try {
      const res = await fetch('/api/friends/unfriend', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ my_id: this.currentUser.id, friend_id: targetId })
      });
      const data = await res.json();
      if (data.status === 'ok') {
        this.showToast('Đã xóa kết bạn thành công!', 'success');
        const btn = document.getElementById('profFriendStatusBtn');
        if (btn) {
          btn.outerHTML = `<button class="btn-primary" id="profActionFriendBtn" onclick="app.sendFriendRequest(${app.jsArg(targetId)})">+ Thêm bạn bè</button>`;
        }
        const countEl = document.getElementById('profFriendCount');
        if (countEl) {
          let cur = parseInt(countEl.textContent || '0', 10);
          if (cur > 0) countEl.textContent = cur - 1;
        }
        await Promise.all([
          this.loadUserFriends(targetId),
          this.loadFriendsAndContacts()
        ]);
        if (document.getElementById('profileActionsRow')) {
          await this.initProfilePage(targetId);
        }
      } else {
        this.showToast(data.message || 'Không thể xóa bạn bè!', 'error');
      }
    } catch (e) {
      this.showToast('Lỗi: ' + e.message, 'error');
    }
  }

  // =========================================================================
  // HỦY GỬI LỜI MỜI KẾT BẠN (TRÊN TRANG PROFILE & BẢNG GỢI Ý)
  // =========================================================================
  async handleCancelFriendRequest(targetId, targetName) {
    if (!this.currentUser) return;

    // Giải quyết tên thật: tránh hiển thị ID dạng ACC000009 (Hình 2)
    let displayName = (targetName || '').trim();
    if (!displayName || displayName === targetId || displayName.startsWith('ACC')) {
      const sugHeading = document.querySelector(`#sug-${targetId} h4`);
      const feedSugHeading = document.querySelector(`#feed-sug-${targetId} .feed-suggestion-name`);
      const profNameEl = document.getElementById('profFullname');
      if (sugHeading && sugHeading.textContent.trim()) {
        displayName = sugHeading.textContent.trim();
      } else if (feedSugHeading && feedSugHeading.textContent.trim()) {
        displayName = feedSugHeading.textContent.trim();
      } else if (profNameEl && profNameEl.textContent.trim()) {
        displayName = profNameEl.textContent.trim();
      } else {
        try {
          const res = await fetch(`/api/users/profile?user_id=${encodeURIComponent(targetId)}`);
          const d = await res.json();
          if (d.status === 'ok' && d.user && d.user.fullname) {
            displayName = d.user.fullname;
          }
        } catch (_) {}
      }
    }
    if (!displayName || displayName.startsWith('ACC')) {
      displayName = 'người dùng này';
    }

    const ok = await this.showConfirm(
      `Xác nhận hủy gửi lời mời kết bạn với <strong>${this.escapeHtml(displayName)}</strong> không?`,
      'Hủy gửi lời mời',
      'Hủy',
      true
    );
    if (!ok) return;

    try {
      const res = await fetch('/api/friends/cancel', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ my_id: this.currentUser.id, target_id: targetId })
      });
      const data = await res.json();
      if (data.status === 'ok') {
        this.showToast('Đã hủy lời mời kết bạn!', 'success');
        this.refreshSentRequestsSilently();
        // Cập nhật nút trong trang cá nhân (nếu đang xem)
        const btn = document.getElementById('profFriendStatusBtn') || document.getElementById('profActionFriendBtn');
        if (btn) {
          btn.outerHTML = `<button class="btn-primary" id="profActionFriendBtn" onclick="app.sendFriendRequest(${app.jsArg(targetId)}, ${app.jsArg(displayName)})">+ Thêm bạn bè</button>`;
        }
        // Cập nhật thẻ tìm kiếm
        const searchBtnBox = document.getElementById(`search-friend-btn-${targetId}`);
        if (searchBtnBox) {
          searchBtnBox.innerHTML = `<button class="btn-primary" type="button" onclick="app.sendFriendRequest(${app.jsArg(targetId)}, ${app.jsArg(displayName)})">+ Thêm bạn bè</button>`;
        }
        // Cập nhật nút trong suggestion box ngay lập tức (Hình 3)
        const sugBox = document.getElementById(`sug-btn-box-${targetId}`);
        if (sugBox) {
          sugBox.innerHTML = `<button class="btn-add-friend" onclick="app.sendFriendRequest(${app.jsArg(targetId)}, ${app.jsArg(displayName)})">+ Kết bạn</button>`;
        }
        const feedSugBox = document.getElementById(`feed-sug-btn-box-${targetId}`);
        if (feedSugBox) {
          feedSugBox.innerHTML = `<button class="feed-suggestion-action btn-primary" onclick="app.sendFriendRequest(${app.jsArg(targetId)})">♟ Thêm bạn bè</button>`;
        }
        // Reload gợi ý kết bạn & danh bạ để đồng bộ trạng thái
        await this.loadSuggestions();
        this.loadFriendsAndContacts();
        if (window.location.pathname.includes('friends.html')) {
          this.loadPendingFriendRequests();
        }
      } else {
        this.showToast(data.message || 'Không thể hủy lời mời!', 'error');
      }
    } catch (e) {
      this.showToast('Lỗi: ' + e.message, 'error');
    }
  }

  // =========================================================================
  // TYPING INDICATOR — Gửi trạng thái đang nhắn & nhận animation "..."
  // =========================================================================
  startTypingIndicator() {
    const chatInput = document.getElementById('chatInput');
    if (!chatInput) return;
    chatInput.addEventListener('input', () => this.onChatInputChange());
    chatInput.addEventListener('keydown', (e) => {
      if (e.key === 'Enter' && !e.shiftKey) this.stopTyping();
    });
  }

  onChatInputChange() {
    if (!this.currentChatPartner || !this.currentUser) return;
    if (this.currentChatPartner.isGroup) return; // Chỉ 1-1
    if (!this.isTyping) {
      this.isTyping = true;
      this.sendTypingStatus(true);
    }
    clearTimeout(this.typingTimer);
    this.typingTimer = setTimeout(() => this.stopTyping(), 3000);
  }

  stopTyping() {
    if (this.isTyping) {
      this.isTyping = false;
      this.sendTypingStatus(false);
    }
  }

  sendTypingStatus(isTyping) {
    if (!this.currentChatPartner || !this.currentUser) return;
    fetch('/api/users/typing', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        user_id: this.currentUser.id,
        partner_id: this.currentChatPartner.id,
        is_typing: isTyping
      })
    }).catch(() => {});
  }

  startTypingPoll() {
    this.stopTypingPoll();
    if (!this.currentChatPartner || this.currentChatPartner.isGroup) return;
    this.typingPollInterval = setInterval(async () => {
      if (!this.currentChatPartner || !this.currentUser) return;
      try {
        const res = await fetch(`/api/users/typing?user_id=${this.currentChatPartner.id}&partner_id=${this.currentUser.id}`);
        const data = await res.json();
        const indicator = document.getElementById('typingIndicator');
        if (indicator) {
          indicator.style.display = data.is_typing ? 'flex' : 'none';
        }
      } catch (e) {}
    }, 1500);
  }

  stopTypingPoll() {
    if (this.typingPollInterval) {
      clearInterval(this.typingPollInterval);
      this.typingPollInterval = null;
    }
  }

  // =========================================================================
  // MESSAGE RECEIPTS — Read only after an incoming message is visibly on screen.
  // =========================================================================
  resetMessageReadTracking() {
    this.stopObservingVisibleMessages();
    this.pendingReadMessageIds.clear();
    this.readMessageIds.clear();
    if (this.readReceiptFlushTimer) {
      clearTimeout(this.readReceiptFlushTimer);
      this.readReceiptFlushTimer = null;
    }
  }

  stopObservingVisibleMessages() {
    if (this.messageReadObserver) {
      this.messageReadObserver.disconnect();
      this.messageReadObserver = null;
    }
    this.messageReadVisibilityTimers.forEach(timer => clearTimeout(timer));
    this.messageReadVisibilityTimers.clear();
  }

  isMessageRowVisible(row, root) {
    if (!row?.isConnected || !root?.isConnected || document.hidden) return false;
    const rowRect = row.getBoundingClientRect();
    const rootRect = root.getBoundingClientRect();
    const visibleHeight = Math.max(0, Math.min(rowRect.bottom, rootRect.bottom) - Math.max(rowRect.top, rootRect.top));
    const visibleWidth = Math.max(0, Math.min(rowRect.right, rootRect.right) - Math.max(rowRect.left, rootRect.left));
    return visibleHeight >= Math.min(80, Math.max(16, rowRect.height * 0.25)) && visibleWidth >= 48;
  }

  updateReadReceipt() {
    this.stopObservingVisibleMessages();
    const partner = this.currentChatPartner;
    const root = document.getElementById('chatBody');
    const list = document.getElementById('messagesList');
    const chatWindow = document.getElementById('chatWindow');
    if (!partner || !root || !list || document.hidden || !chatWindow || chatWindow.style.display === 'none' ||
        !('IntersectionObserver' in window)) return;

    this.messageReadObserver = new IntersectionObserver(entries => {
      for (const entry of entries) {
        const row = entry.target;
        const messageId = Number(row.id.replace(/^msg-/, ''));
        if (!entry.isIntersecting || !Number.isInteger(messageId) || messageId <= 0 ||
            this.readMessageIds.has(messageId) || this.pendingReadMessageIds.has(messageId)) {
          const timer = this.messageReadVisibilityTimers.get(row);
          if (timer) clearTimeout(timer);
          this.messageReadVisibilityTimers.delete(row);
          continue;
        }
        if (this.messageReadVisibilityTimers.has(row)) continue;
        const timer = setTimeout(() => {
          this.messageReadVisibilityTimers.delete(row);
          if (!this.currentChatPartner || this.currentChatPartner.id !== partner.id ||
              !!this.currentChatPartner.isGroup !== !!partner.isGroup || !this.isMessageRowVisible(row, root)) return;
          this.pendingReadMessageIds.add(messageId);
          if (this.readReceiptFlushTimer) clearTimeout(this.readReceiptFlushTimer);
          this.readReceiptFlushTimer = setTimeout(() => this.flushVisibleMessageReads(), 120);
        }, 300);
        this.messageReadVisibilityTimers.set(row, timer);
      }
    }, { root, threshold: [0, 0.2] });

    list.querySelectorAll('.message-row.other[id^="msg-"]').forEach(row => {
      const messageId = Number(row.id.replace(/^msg-/, ''));
      if (Number.isInteger(messageId) && messageId > 0 && !this.readMessageIds.has(messageId)) {
        this.messageReadObserver.observe(row);
      }
    });
  }

  async flushVisibleMessageReads() {
    this.readReceiptFlushTimer = null;
    const partner = this.currentChatPartner;
    if (!partner || !this.currentUser || document.hidden || this.readReceiptRequestInFlight) return;
    const ids = [...this.pendingReadMessageIds].filter(id => !this.readMessageIds.has(id)).slice(0, 100);
    if (!ids.length) return;
    this.readReceiptRequestInFlight = true;
    try {
      const res = await fetch('/api/messages/receipts/read', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          user_id: this.currentUser.id,
          conversation_id: partner.id,
          is_group: !!partner.isGroup,
          message_ids: ids
        })
      });
      const data = await res.json();
      if (!res.ok || data.status !== 'ok') throw new Error(data.message || 'Không thể cập nhật trạng thái đã xem.');
      ids.forEach(id => {
        this.pendingReadMessageIds.delete(id);
        this.readMessageIds.add(id);
      });
    } catch (error) {
      console.error('Không thể gửi trạng thái đã xem:', error);
      this.readReceiptFlushTimer = setTimeout(() => this.flushVisibleMessageReads(), 3000);
    } finally {
      this.readReceiptRequestInFlight = false;
      if (this.pendingReadMessageIds.size && !this.readReceiptFlushTimer) {
        this.readReceiptFlushTimer = setTimeout(() => this.flushVisibleMessageReads(), 120);
      }
    }
  }

  startReadReceiptPoll() {
    this.updateReadReceipt();
  }

  stopReadReceiptPoll() {
    if (this.msgReadPollInterval) {
      clearInterval(this.msgReadPollInterval);
      this.msgReadPollInterval = null;
    }
    this.stopObservingVisibleMessages();
    if (this.readReceiptFlushTimer) {
      clearTimeout(this.readReceiptFlushTimer);
      this.readReceiptFlushTimer = null;
    }
  }

  ensureMessageReceiptsModal() {
    let modal = document.getElementById('messageReceiptsModal');
    if (modal) return modal;
    const wrapper = document.createElement('div');
    wrapper.innerHTML = `
      <div class="message-receipts-overlay" id="messageReceiptsModal" style="display:none" onclick="if(event.target===this) app.closeMessageReceiptsModal()">
        <section class="message-receipts-dialog" role="dialog" aria-modal="true" aria-labelledby="messageReceiptsTitle">
          <header class="message-receipts-header">
            <h2 id="messageReceiptsTitle">Thông tin tin nhắn</h2>
            <button type="button" class="btn-icon" onclick="app.closeMessageReceiptsModal()" aria-label="Đóng">✕</button>
          </header>
          <div class="message-receipts-preview-box" id="messageReceiptsPreviewBox">
            <div class="message-receipts-card">
              <div class="message-receipts-card-sender" id="messageReceiptsSender">...</div>
              <div class="message-receipts-card-reply" id="messageReceiptsReply" style="display:none;"></div>
              <div class="message-receipts-card-media" id="messageReceiptsMedia" style="display:none;"></div>
              <div class="message-receipts-card-content" id="messageReceiptsContent"></div>
              <div class="message-receipts-card-time" id="messageReceiptsTime"></div>
            </div>
          </div>
          <div class="message-receipts-body">
            <section>
              <h3 class="message-receipts-section-title" id="messageReceiptsSeenTitle">Đã xem (0)</h3>
              <div id="messageReceiptsSeenList" class="message-receipts-user-grid"></div>
            </section>
            <section>
              <h3 class="message-receipts-section-title" id="messageReceiptsDeliveredTitle">Đã nhận (0)</h3>
              <div id="messageReceiptsDeliveredList" class="message-receipts-user-grid"></div>
            </section>
          </div>
        </section>
      </div>
    `;
    modal = wrapper.firstElementChild;
    if (!modal) throw new Error('Không thể tạo bảng thông tin tin nhắn.');
    document.body.appendChild(modal);
    return modal;
  }

  renderReceiptUsers(receipts, timeKey, statusLabel) {
    if (!receipts.length) return '<div class="message-receipts-empty">Chưa có thành viên</div>';
    return receipts.map(receipt => {
      const time = receipt[timeKey] ? this.formatChatMessageTime(receipt[timeKey]) : '';
      const tooltip = time ? `${receipt.name || 'Người dùng'} · ${statusLabel} ${time}` : (receipt.name || 'Người dùng');
      return `
        <div class="message-receipts-user-item" title="${this.escapeHtml(tooltip)}">
          <img src="${this.escapeHtml(receipt.avatar || '/img/default-avatar.svg')}" alt="">
          <div class="message-receipts-user-info">
            <span class="message-receipts-user-name">${this.escapeHtml(receipt.name || 'Người dùng')}</span>
            <span class="message-receipts-user-time">${this.escapeHtml(time || statusLabel)}</span>
          </div>
        </div>
      `;
    }).join('');
  }

  async openMessageReceiptsModal(messageId, event) {
    event?.stopPropagation();
    let message = this.msgCache[messageId] || null;
    if (!this.currentUser) return;
    const isGroup = !!(this.currentChatPartner && this.currentChatPartner.isGroup);

    const modal = this.ensureMessageReceiptsModal();
    modal.style.display = 'flex';

    // Đóng dropdown more menu nếu đang mở
    const moreMenu = document.getElementById(`msg-more-${messageId}`);
    if (moreMenu) moreMenu.classList.remove('show');

    // Tải dữ liệu biên lai mới nhất từ server
    try {
      const res = await fetch(`/api/messages/receipts?message_id=${encodeURIComponent(messageId)}&is_group=${isGroup}&user_id=${encodeURIComponent(this.currentUser.id)}`);
      const data = await res.json();
      if (data.status === 'ok') {
        if (data.message && data.message.id) {
          message = data.message;
          this.msgCache[messageId] = message;
        }
        if (Array.isArray(data.receipts)) {
          if (!message) message = { id: messageId };
          message.receipts = data.receipts;
        }
      }
    } catch (e) {
      console.warn('Lỗi tải thông tin tin nhắn:', e);
    }

    if (!message) return;

    // Người gửi tin nhắn không xuất hiện trong danh sách người đã xem của chính họ
    const senderId = message.sender_id;
    const senderName = message.sender_name || (message.sender_id === this.currentUser.id ? 'Bạn' : (this.currentChatPartner?.name || 'Người gửi'));
    const senderEl = document.getElementById('messageReceiptsSender');
    if (senderEl) senderEl.textContent = senderName;

    // Quote trả lời (nếu có)
    const replyEl = document.getElementById('messageReceiptsReply');
    if (replyEl) {
      if (message.reply_to_text) {
        replyEl.style.display = 'flex';
        replyEl.innerHTML = `<span>↩️</span> <span style="overflow:hidden;text-overflow:ellipsis;white-space:nowrap;">${this.escapeHtml(message.reply_to_text)}</span>`;
      } else {
        replyEl.style.display = 'none';
        replyEl.innerHTML = '';
      }
    }

    // Media đính kèm (nếu có)
    const mediaEl = document.getElementById('messageReceiptsMedia');
    if (mediaEl) {
      if (message.media_type === 'IMAGE' && message.media_url) {
        mediaEl.style.display = 'block';
        mediaEl.innerHTML = `<img src="${this.escapeHtml(message.media_url)}" alt="Hình ảnh" style="max-height:140px;border-radius:6px;object-fit:cover;">`;
      } else if (message.media_type === 'VIDEO' && message.media_url) {
        mediaEl.style.display = 'block';
        mediaEl.innerHTML = `<video src="${this.escapeHtml(message.media_url)}" style="max-height:140px;border-radius:6px;" controls></video>`;
      } else {
        mediaEl.style.display = 'none';
        mediaEl.innerHTML = '';
      }
    }

    // Nội dung tin nhắn
    const contentEl = document.getElementById('messageReceiptsContent');
    if (contentEl) {
      contentEl.textContent = message.is_revoked ? 'Tin nhắn đã được thu hồi' : (message.content || '');
    }

    // Thời gian gửi
    const timeEl = document.getElementById('messageReceiptsTime');
    if (timeEl) {
      timeEl.textContent = message.created_at ? this.formatChatMessageTooltip(message.created_at) : '';
    }

    // Lọc danh sách biên lai
    const recipients = (Array.isArray(message.receipts) ? message.receipts : [])
      .filter(r => r.user_id && r.user_id !== senderId);

    // Đã xem: read_at != ''
    const seen = recipients.filter(r => r.read_at)
      .sort((a, b) => String(b.read_at).localeCompare(String(a.read_at)));

    // Đã nhận: delivered_at != '' AND read_at == '' (đã nhận nhưng chưa xem)
    const delivered = recipients.filter(r => r.delivered_at && !r.read_at)
      .sort((a, b) => String(b.delivered_at).localeCompare(String(a.delivered_at)));

    const seenTitle = document.getElementById('messageReceiptsSeenTitle');
    const deliveredTitle = document.getElementById('messageReceiptsDeliveredTitle');
    const seenList = document.getElementById('messageReceiptsSeenList');
    const deliveredList = document.getElementById('messageReceiptsDeliveredList');

    if (seenTitle) seenTitle.textContent = `Đã xem (${seen.length})`;
    if (deliveredTitle) deliveredTitle.textContent = `Đã nhận (${delivered.length})`;
    if (seenList) seenList.innerHTML = this.renderReceiptUsers(seen, 'read_at', 'Đã xem');
    if (deliveredList) deliveredList.innerHTML = this.renderReceiptUsers(delivered, 'delivered_at', 'Đã nhận');
  }

  closeMessageReceiptsModal() {
    const modal = document.getElementById('messageReceiptsModal');
    if (modal) modal.style.display = 'none';
  }

  // =========================================================================
  // THAY ĐỔI AVATAR NHÓM
  // =========================================================================
  async changeGroupAvatar() {
    if (!this.currentChatPartner || !this.currentChatPartner.isGroup) return;
    const input = document.createElement('input');
    input.type = 'file';
    input.accept = 'image/*';
    input.onchange = async () => {
      if (!input.files || !input.files[0]) return;
      const file = input.files[0];
      const reader = new FileReader();
      reader.onload = async (e) => {
        const dataUrl = e.target.result;
        try {
          const res = await fetch('/api/groups/update-avatar', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
              group_id: this.currentChatPartner.id,
              user_id: this.currentUser.id,
              avatar: dataUrl
            })
          });
          const data = await res.json();
          if (data.status === 'ok') {
            this.showToast('Đã cập nhật ảnh đại diện nhóm!', 'success');
            this.currentChatPartner.avatar = dataUrl;
            const chatAvEl = document.getElementById('chatPartnerAvatar');
            if (chatAvEl) chatAvEl.src = dataUrl;
            this.loadFriendsAndContacts();
          } else {
            this.showToast(data.message || 'Lỗi cập nhật ảnh nhóm', 'error');
          }
        } catch(e) {
          this.showToast('Lỗi: ' + e.message, 'error');
        }
      };
      reader.readAsDataURL(file);
    };
    input.click();
  }

  // =========================================================================
  // LIGHTBOX XEM ẢNH LỚN (avatar nhóm / ảnh bài viết)
  // =========================================================================
  openImageLightbox(src, event) {
    if (event) {
      event.preventDefault();
      event.stopPropagation();
    }
    if (!src || src === '/img/default-avatar.svg') {
      // Vẫn cho xem ảnh mặc định
    }
    const overlay = document.getElementById('imageLightbox');
    const img = document.getElementById('imageLightboxImg');
    if (!overlay || !img) return;
    img.src = src;
    this._lightboxZoom = 1;
    img.style.transform = 'scale(1)';
    overlay.classList.add('active');
    document.body.style.overflow = 'hidden';
  }

  closeImageLightbox() {
    const overlay = document.getElementById('imageLightbox');
    if (overlay) overlay.classList.remove('active');
    document.body.style.overflow = '';
    this._lightboxZoom = 1;
  }

  zoomImageLightbox(delta) {
    const img = document.getElementById('imageLightboxImg');
    if (!img) return;
    this._lightboxZoom = Math.max(0.4, Math.min(4, (this._lightboxZoom || 1) + delta));
    img.style.transform = `scale(${this._lightboxZoom})`;
  }

  resetImageLightboxZoom() {
    const img = document.getElementById('imageLightboxImg');
    if (!img) return;
    this._lightboxZoom = 1;
    img.style.transform = 'scale(1)';
  }

  openMediaViewer(type, src, event) {
    if (event) {
      event.preventDefault();
      event.stopPropagation();
    }
    if (!src || !['IMAGE', 'VIDEO'].includes(type)) return;

    let overlay = document.getElementById('mediaViewer');
    if (!overlay) {
      overlay = document.createElement('div');
      overlay.id = 'mediaViewer';
      overlay.className = 'media-viewer-overlay';
      overlay.setAttribute('role', 'dialog');
      overlay.setAttribute('aria-modal', 'true');
      overlay.setAttribute('aria-label', 'Xem nội dung đa phương tiện');
      overlay.addEventListener('click', e => {
        if (e.target === overlay) this.closeMediaViewer();
      });
      document.body.appendChild(overlay);
    }

    const safeSrc = this.escapeHtml(src);
    overlay.innerHTML = `
      <button type="button" class="media-viewer-close" aria-label="Đóng" onclick="app.closeMediaViewer()">✕</button>
      ${type === 'IMAGE'
        ? `<img class="media-viewer-content" src="${safeSrc}" alt="Ảnh phóng to">`
        : `<video class="media-viewer-content" src="${safeSrc}" controls autoplay playsinline></video>`}
    `;
    overlay.classList.add('active');
    document.body.style.overflow = 'hidden';
  }

  closeMediaViewer() {
    const overlay = document.getElementById('mediaViewer');
    if (!overlay) return;
    const video = overlay.querySelector('video');
    if (video) {
      video.pause();
      video.removeAttribute('src');
      video.load();
    }
    overlay.classList.remove('active');
    overlay.innerHTML = '';
    document.body.style.overflow = '';
  }

  onChatAvatarClick(event) {
    if (event) {
      event.preventDefault();
      event.stopPropagation();
    }
    const av = document.getElementById('chatPartnerAvatar');
    const src = av ? av.src : '';
    if (!src) return;
    // Luôn mở lightbox khi click avatar (cả user và group) — không điều hướng profile
    this.openImageLightbox(src, event);
  }

  onChatPartnerNameClick(event) {
    if (event) event.stopPropagation();
    if (!this.currentChatPartner) return;
    if (this.currentChatPartner.isGroup) {
      // Nhóm: không mở profile cá nhân
      return;
    }
    this.openUserProfile(this.currentChatPartner.user_token || this.currentChatPartner.id);
  }

  // --- Menu avatar thành viên trong nhóm chat ---
  openGroupMemberAvatarMenu(event, userId, userName, userAvatar) {
    if (event) {
      event.preventDefault();
      event.stopPropagation();
    }
    this.closeGroupMemberAvatarMenu();
    const menu = document.createElement('div');
    menu.id = 'groupMemberAvatarMenu';
    menu.className = 'group-member-avatar-menu';
    menu.innerHTML = `
      <button type="button" onclick="app.startPrivateChatFromGroup(${app.jsArg(userId)}, ${app.jsArg(userName)}, ${app.jsArg(userAvatar || '')}); app.closeGroupMemberAvatarMenu();">💬 Gửi tin nhắn</button>
      <button type="button" onclick="app.openUserProfile(${app.jsArg(userId)}); app.closeGroupMemberAvatarMenu();">👤 Xem trang cá nhân</button>
    `;
    document.body.appendChild(menu);
    const x = Math.min((event?.clientX || 80), window.innerWidth - 220);
    const y = Math.min((event?.clientY || 80), window.innerHeight - 120);
    menu.style.left = x + 'px';
    menu.style.top = y + 'px';
    setTimeout(() => {
      const closer = (e) => {
        if (!e.target.closest('#groupMemberAvatarMenu')) {
          this.closeGroupMemberAvatarMenu();
          document.removeEventListener('mousedown', closer);
        }
      };
      document.addEventListener('mousedown', closer);
    }, 0);
  }

  closeGroupMemberAvatarMenu() {
    document.getElementById('groupMemberAvatarMenu')?.remove();
  }

  startPrivateChatFromGroup(userId, userName, userAvatar) {
    if (!userId) return;
    if (typeof this.openChatWith === 'function') {
      this.openChatWith(userId, userName || 'Người dùng', userAvatar || '/img/default-avatar.svg', userId);
    } else {
      window.location.href = `messenger.html?chat=${encodeURIComponent(userId)}`;
    }
  }

  // --- GIPHY GIF picker ---
  openGifPicker(target = 'comment') {
    // target: 'comment' | 'chat' | 'post'
    this._gifPickerTarget = target;
    let overlay = document.getElementById('gifPickerOverlay');
    if (!overlay) {
      overlay = document.createElement('div');
      overlay.id = 'gifPickerOverlay';
      overlay.innerHTML = `
        <div class="gif-picker-modal" role="dialog" aria-label="Chọn GIF">
          <div class="gif-picker-header">
            <strong>GIF</strong>
            <button type="button" class="gif-picker-close" onclick="app.closeGifPicker()">✕</button>
          </div>
          <input id="gifSearchInput" type="search" placeholder="Tìm kiếm GIF..." oninput="app.searchGifs(this.value)">
          <div id="gifResults" class="gif-results"><div class="gif-loading">Đang tải GIF phổ biến...</div></div>
        </div>`;
      overlay.addEventListener('click', (e) => { if (e.target === overlay) this.closeGifPicker(); });
      document.body.appendChild(overlay);
    }
    overlay.classList.add('show');
    this.searchGifs('');
    setTimeout(() => document.getElementById('gifSearchInput')?.focus(), 50);
  }

  closeGifPicker() {
    document.getElementById('gifPickerOverlay')?.classList.remove('show');
  }

  async searchGifs(query) {
    const box = document.getElementById('gifResults');
    if (!box) return;
    box.innerHTML = '<div class="gif-loading">Đang tải...</div>';
    const q = (query || '').trim();
    // Dùng Giphy public demo key + trending/search
    const key = 'dc6zaTOxFJmzC';
    const url = q
      ? `https://api.giphy.com/v1/gifs/search?api_key=${key}&q=${encodeURIComponent(q)}&limit=24&rating=pg-13`
      : `https://api.giphy.com/v1/gifs/trending?api_key=${key}&limit=24&rating=pg-13`;
    try {
      const res = await fetch(url);
      const data = await res.json();
      const items = (data.data || []).map(g => {
        const still = g.images?.fixed_width_small?.url || g.images?.preview_gif?.url || '';
        const full = g.images?.original?.url || g.images?.downsized?.url || still;
        return { still, full, title: g.title || 'GIF' };
      }).filter(g => g.full);
      if (!items.length) {
        // Fallback mẫu nếu API khóa / lỗi mạng
        box.innerHTML = this._fallbackGifs().map(g => this._gifItemHtml(g)).join('');
        return;
      }
      box.innerHTML = items.map(g => this._gifItemHtml(g)).join('');
    } catch (e) {
      box.innerHTML = this._fallbackGifs().map(g => this._gifItemHtml(g)).join('');
    }
  }

  _gifItemHtml(g) {
    const full = this.escapeHtml(g.full);
    const still = this.escapeHtml(g.still || g.full);
    const title = this.escapeHtml(g.title || 'GIF');
    return `<button type="button" class="gif-item" title="${title}" onclick="app.selectGif(${app.jsArg(full)})"><img src="${still}" alt="${title}" loading="lazy"></button>`;
  }

  _fallbackGifs() {
    return [
      { title: 'Happy', full: 'https://media.giphy.com/media/111ebonMs90YLu/giphy.gif', still: 'https://media.giphy.com/media/111ebonMs90YLu/200w.gif' },
      { title: 'Clap', full: 'https://media.giphy.com/media/7rj2ZgttXq3oY/giphy.gif', still: 'https://media.giphy.com/media/7rj2ZgttXq3oY/200w.gif' },
      { title: 'Thumbs up', full: 'https://media.giphy.com/media/3o7abKhOpu0NwenH3O/giphy.gif', still: 'https://media.giphy.com/media/3o7abKhOpu0NwenH3O/200w.gif' },
      { title: 'LOL', full: 'https://media.giphy.com/media/10JhviFuU2gWD6/giphy.gif', still: 'https://media.giphy.com/media/10JhviFuU2gWD6/200w.gif' },
      { title: 'Love', full: 'https://media.giphy.com/media/l0MYt5jPRYRf2oC68/giphy.gif', still: 'https://media.giphy.com/media/l0MYt5jPRYRf2oC68/200w.gif' },
      { title: 'Yes', full: 'https://media.giphy.com/media/3ohzdIuqJoo8QdKlnW/giphy.gif', still: 'https://media.giphy.com/media/3ohzdIuqJoo8QdKlnW/200w.gif' }
    ];
  }

  async selectGif(gifUrl) {
    this.closeGifPicker();
    const target = this._gifPickerTarget || 'comment';
    if (target === 'chat') {
      await this.sendChatGif(gifUrl);
    } else if (target === 'post') {
      this.attachGifToPostComposer(gifUrl);
    } else {
      await this.sendCommentGif(gifUrl);
    }
  }

  async sendChatGif(gifUrl) {
    if (!this.currentUser || !this.currentChatPartner) {
      this.showToast('Hãy mở một cuộc trò chuyện trước.', 'info');
      return;
    }
    const isGroup = !!this.currentChatPartner.isGroup;
    try {
      if (isGroup) {
        const res = await fetch('/api/groups/messages/send', {
          method: 'POST', headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({
            group_id: this.currentChatPartner.id,
            sender_id: this.currentUser.id,
            content: '',
            media_type: 'IMAGE',
            media_url: gifUrl
          })
        });
        const data = await this.readApiResponse(res);
        if (!res.ok || data.status !== 'ok') throw new Error(data.message || 'Không gửi được GIF');
      } else {
        const res = await fetch('/api/messages/send', {
          method: 'POST', headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({
            sender_id: this.currentUser.id,
            recipient_id: this.currentChatPartner.id,
            content: '',
            media_type: 'IMAGE',
            media_url: gifUrl
          })
        });
        const data = await this.readApiResponse(res);
        if (!res.ok || data.status !== 'ok') throw new Error(data.message || 'Không gửi được GIF');
      }
      // Làm mới khung chat
      if (isGroup) await this.loadGroupMessages(this.currentChatPartner.id);
      else await this.loadInitialMessages();
    } catch (e) {
      this.showToast(e.message || 'Không gửi được GIF', 'error');
    }
  }

  attachGifToPostComposer(gifUrl) {
    this.currentMedia = { type: 'IMAGE', url: gifUrl };
    this.pendingPostMedia = { type: 'IMAGE', url: gifUrl };
    const gallery = document.getElementById('postImagePreview');
    if (gallery) {
      gallery.style.display = 'block';
      gallery.innerHTML = `<img src="${this.escapeHtml(gifUrl)}" alt="GIF" style="max-width:100%;border-radius:10px;">`;
    }
    const video = document.getElementById('postVideoPreview');
    if (video) {
      video.style.display = 'none';
      video.removeAttribute('src');
    }
    this.showToast('Đã chọn GIF cho bài viết. Nhấn Đăng để gửi.', 'success');
  }

  async sendCommentGif(gifUrl) {
    // Bình luận GIF trên bài đang mở modal
    const postId = this.activeModalPostId || this._commentGifPostId;
    if (!postId || !this.currentUser) {
      this.showToast('Hãy mở bình luận của một bài viết trước.', 'info');
      return;
    }
    try {
      const res = await fetch('/api/posts/comment', {
        method: 'POST', headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          post_id: postId,
          author_id: this.currentUser.id,
          content: '',
          media_type: 'IMAGE',
          media_url: gifUrl
        })
      });
      const data = await this.readApiResponse(res);
      if (!res.ok || data.status !== 'ok') throw new Error(data.message || 'Không gửi được GIF bình luận');
      this.showToast('Đã gửi GIF!', 'success');
      if (typeof this.reloadCommentModal === 'function') this.reloadCommentModal(postId);
      else if (typeof this.openCommentModal === 'function') this.openCommentModal(postId);
    } catch (e) {
      this.showToast(e.message || 'Không gửi được GIF bình luận', 'error');
    }
  }
}

document.addEventListener('DOMContentLoaded', () => {
  window.app = new AppClient();
});


/* search-user-menu-outside-click */
document.addEventListener('click', (e) => {
  if (!e.target.closest('.search-user-card .suc-menu') && !e.target.closest('.suc-menu-btn')) {
    if (window.app && typeof app.closeAllSearchUserMenus === 'function') app.closeAllSearchUserMenus();
  }
});
