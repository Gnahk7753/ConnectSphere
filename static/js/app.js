/**
 * Mạng Xã Hội MIni - C++ Web Platform & SQLite Engine Client
 * Bao gồm đầy đủ tính năng: Profile riêng, Modal bình luận, Nhắn tin nhóm,
 * Chia sẻ vào chat, Gợi ý tìm kiếm thời gian thực, Bộ lọc bạn bè, Thông báo.
 */

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
    this.replyingToCommentId = null;
    this.replyingToAuthorName = null;
    this.currentSuggestionFilter = 'ALL';
    this.currentSuggestionLimit = 5;
    this.selectedHobbies = [];
    this.hobbyTargetInputId = '';
    this.currentContactFilter = 'all';
    this.allContactsCache = null;
    this.adminReportsCache = null;
    this.adminPostsCache = null;
    this.typingTimer = null;
    this.isTyping = false;
    this.typingPollInterval = null;
    this.msgReadPollInterval = null;
    this.messagePollInterval = null;
    this.replyingToMessage = null;
    this.lastRenderedMsgSignature = '';

    // Phân trang bài viết (Feed: 10 ban đầu, load 7; Profile: 5 ban đầu, load 5)
    this.feedLimit = 10;
    this.feedOffset = 0;
    this.feedHasMore = true;
    this.isLoadingFeed = false;
    this.profileLimit = 5;
    this.profileOffset = 0;
    this.profileHasMore = true;
    this.isLoadingProfile = false;
    this.profileUserId = null;

    this.init();
  }

  init() {
    this.initTheme();
    this.initVietnamDataOptions();
    this.checkLoginStatus();
    this.setupEventListeners();
    this.setupInfiniteScroll();
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
    // Nếu người dùng chưa từng đăng nhập (lần đầu vào web), tự động đăng nhập tài khoản mẫu Nguyễn Minh Khoa để test ngay
    if (!stored && !sessionStorage.getItem('cs_logged_out')) {
      const defaultUser = {
        id: "ACC000001",
        username: "khoanm",
        email: "khoanm@gmail.com",
        fullname: "Nguyễn Minh Khoa",
        avatar: "/img/default-avatar.svg",
        bio: "Sống chậm lại và tận hưởng từng khoảnh khắc cuộc sống.",
        hometown: "Đà Nẵng",
        school: "Đại học Bách Khoa Hà Nội",
        workplace: "Tiki",
        interests: "Đọc sách;Nghe nhạc;Lập trình;Bóng đá",
        role: "USER",
        status: "ACTIVE",
        user_token: "504258007eab0d25"
      };
      localStorage.setItem('cs_user', JSON.stringify(defaultUser));
      stored = JSON.stringify(defaultUser);
    }

    if (stored) {
      try {
        this.currentUser = JSON.parse(stored);
        this.renderUserUI();
        this.startHeartbeat();

        const authModal = document.getElementById('authModal');
        if (authModal) authModal.style.display = 'none';

        // Tải các widget nếu đang ở trang chủ index.html
        if (document.getElementById('postsList') && !window.location.pathname.includes('profile.html')) {
          this.loadFeed();
          this.loadSuggestions();
          this.loadFriendsAndContacts();
          this.loadNotifications();
        } else if (window.location.pathname.includes('profile.html')) {
          const urlParams = new URLSearchParams(window.location.search);
          const targetUserId = urlParams.get('id') || this.currentUser.id;
          this.initProfilePage(targetUserId);
          this.loadNotifications();
        }

        // Hiện nút Admin nếu có quyền ADMIN
        if (this.currentUser.role === 'ADMIN') {
          const admBtn = document.getElementById('adminPanelBtn');
          if (admBtn) admBtn.style.display = 'flex';
          const sideAdm = document.getElementById('sideAdminItem');
          if (sideAdm) sideAdm.style.display = 'block';
        }
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

  startHeartbeat() {
    if (this.heartbeatInterval) clearInterval(this.heartbeatInterval);
    const sendPulse = () => {
      if (!this.currentUser) return;
      fetch('/api/users/heartbeat', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ user_id: this.currentUser.id })
      }).then(() => {
        if (document.getElementById('contactsContainer')) {
          this.loadFriendsAndContacts();
        }
      }).catch(() => {});
    };
    sendPulse();
    this.heartbeatInterval = setInterval(sendPulse, 30000); // 30 giây gửi 1 lần & làm mới trạng thái online
  }

  setupEventListeners() {
    const logoutBtn = document.getElementById('logoutBtn');
    if (logoutBtn) {
      logoutBtn.addEventListener('click', () => this.handleLogout());
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

      // Đóng menu bài viết và menu comment khi bấm ra ngoài
      if (!e.target.closest('.post-more-wrapper')) {
        document.querySelectorAll('.post-more-dropdown.show').forEach(el => el.classList.remove('show'));
      }
      if (!e.target.closest('.comment-more-wrapper')) {
        document.querySelectorAll('.comment-more-dropdown.show').forEach(el => el.classList.remove('show'));
      }
      if (!e.target.closest('.msg-circle-btn') && !e.target.closest('.msg-more-dropdown') && !e.target.closest('.msg-reaction-picker')) {
        this.closeAllMessageDropdowns();
      }
    });
  }

  async handleLogout() {
    const confirmed = await this.showConfirm('Bạn có chắc chắn muốn đăng xuất?');
    if (!confirmed) return;
    localStorage.removeItem('cs_user');
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
      this.isLoadingFeed = false;
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
          container.innerHTML = data.posts.map(post => this.renderPostCard(post)).join('') +
            '<div id="feedScrollLoader" class="feed-scroll-loader" style="display:none;"><div class="spinner"></div> Đang tải thêm bài viết...</div>';
        }
      } catch (err) {
        container.innerHTML = '<div style="text-align: center; padding: 2rem; color: var(--accent-red);">Không thể tải bảng tin!</div>';
      }
    } else {
      if (!this.feedHasMore || this.isLoadingFeed) return;
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
      } catch (err) {
        console.error('Lỗi tải thêm bài viết feed:', err);
      } finally {
        if (loader) loader.style.display = 'none';
        this.isLoadingFeed = false;
      }
    }
  }

  renderPostCard(p) {
    if (!this.postsMap) this.postsMap = {};
    this.postsMap[p.id] = p;

    // Xử lý icon nút bày tỏ cảm xúc của tôi
    let myReactionIcon = '🤍';
    let myReactionClass = '';
    let myReactionLabel = 'Thích';

    if (p.my_reaction === 'LOVE') { myReactionIcon = '❤️'; myReactionClass = 'reacted-love'; myReactionLabel = 'Yêu thích'; }
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
          if (r.type === 'LOVE') emoji = '❤️';
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
        <div class="reaction-cluster-box" onclick="app.openPostReactionsModal('${p.id}')" title="${this.escapeHtml(tooltipText)}">
          <div class="reaction-cluster-icons">${iconsHtml}</div>
          <span>${totalCount}</span>
          <div class="reaction-names-tooltip">${this.escapeHtml(tooltipText)}</div>
        </div>
      `;
    } else {
      reactionsClusterHtml = `<span style="color: var(--text-muted); font-size: 0.85rem;">Hãy là người đầu tiên bày tỏ cảm xúc</span>`;
    }

    // Media (Ảnh hoặc Video)
    let mediaHtml = '';
    if (p.media_type === 'IMAGE' && p.media_url) {
      mediaHtml = `<div class="post-media-box"><img src="${p.media_url}" alt="Post image"></div>`;
    } else if (p.media_type === 'VIDEO' && p.media_url) {
      mediaHtml = `<div class="post-media-box"><video src="${p.media_url}" controls></video></div>`;
    }

    // Thời gian đăng bài theo định dạng console timeAgo
    const timeDisplay = this.formatTimeAgo(p.created_at);
    const commentCount = (p.comments && Array.isArray(p.comments)) ? p.comments.length : 0;
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
        let origMediaHtml = '';
        if (op.media_type === 'IMAGE' && op.media_url) {
          origMediaHtml = `<div class="repost-orig-media"><img src="${op.media_url}" alt="Ảnh bài viết gốc" onclick="window.open('${op.media_url}')"></div>`;
        } else if (op.media_type === 'VIDEO' && op.media_url) {
          origMediaHtml = `<div class="repost-orig-media"><video src="${op.media_url}" controls></video></div>`;
        }

        origHtml = `
          <div class="repost-original-card">
            <div class="repost-orig-header">
              <div class="repost-orig-author-info" onclick="app.openUserProfile('${op.author_token || op.author_id}')" style="cursor: pointer;">
                <img src="${op.author_avatar || '/img/default-avatar.svg'}" class="repost-orig-avatar" alt="Avatar">
                <div>
                  <div class="repost-orig-name">${this.escapeHtml(op.author_name)}</div>
                  <div class="repost-orig-time">🕒 ${this.formatTimeAgo(op.created_at)}</div>
                </div>
              </div>
              <button class="btn-view-orig-post" onclick="app.openCommentModal('${op.id}')" title="Xem chi tiết bài viết gốc">
                Xem gốc ↗
              </button>
            </div>
            ${op.title ? `<div class="repost-orig-title">${this.escapeHtml(op.title)}</div>` : ''}
            <div class="repost-orig-content">${this.escapeHtml(op.content || '')}</div>
            ${origMediaHtml}
            <div class="repost-orig-footer">
              <span onclick="app.openCommentModal('${op.id}')" style="cursor: pointer;">💬 ${op.comments_count || 0} bình luận &bull; ❤️ ${op.total_reactions || 0} cảm xúc</span>
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
          <div class="post-header-left" onclick="app.openUserProfile('${profileTarget}')" style="cursor: pointer;">
            <img src="${p.author_avatar || '/img/default-avatar.svg'}" alt="Avatar">
            <div>
              <div class="post-author-name">${this.escapeHtml(p.author_name)} ${repostBadgeHtml}${p.category_label ? `<span class="post-category-badge" title="Chủ đề">${this.escapeHtml(p.category_label)}</span>` : ''}${p.moderation_action === 'FLAG' ? `<span class="post-mod-flag" title="${this.escapeHtml(p.moderation_reason || '')}">⚠️ Kiểm duyệt</span>` : ''}</div>
              <div class="post-time">🕒 ${timeDisplay} <span class="post-visibility-badge" title="Chế độ hiển thị">${visIcon}</span></div>
            </div>
          </div>
          <div class="post-more-wrapper">
            <button class="post-more-btn" onclick="app.togglePostMenu(event, '${p.id}')" title="Tùy chọn">•••</button>
            <div class="post-more-dropdown" id="post-menu-${p.id}">
              ${isAuthor ? `
                ${!isRepost ? `
                  <button class="post-dropdown-item" onclick="app.openEditPostModal('${p.id}')">
                    <span>✏️</span> Chỉnh sửa bài viết
                  </button>
                ` : ''}
                <button class="post-dropdown-item text-danger" onclick="app.deleteMyPost('${p.id}')">
                  <span>🗑️</span> Xóa bài đăng
                </button>
              ` : `
                <button class="post-dropdown-item" onclick="app.hidePost('${p.id}')">
                  <span>👁️</span> Ẩn bài đăng
                </button>
              `}
              <button class="post-dropdown-item" onclick="app.openReportModal('', '${p.id}')">
                <span>🚩</span> Báo cáo
              </button>
            </div>
          </div>
        </div>

        ${bodyContentHtml}

        <!-- Thống kê: Cụm icon cảm xúc (Hình 1) và Số lượng bình luận -->
        <div class="post-stats">
          <div>${reactionsClusterHtml}</div>
          <div onclick="app.openCommentModal('${p.id}')" style="cursor: pointer;">
            💬 ${commentCount} bình luận
          </div>
        </div>

        <!-- Các nút tương tác: Thả cảm xúc, Xem bình luận (Mở Modal Hình 2), Đăng lại (Repost), Chia sẻ vào chat -->
        <div class="post-actions">
          <div class="reaction-btn-wrapper">
            <button class="btn-post-action ${myReactionClass}" onclick="app.toggleLike('${p.id}', '${p.my_reaction}')">
              <span>${myReactionIcon}</span>
              <span>${myReactionLabel}</span>
            </button>
            <div class="reaction-popover">
              <span class="reaction-item" onclick="app.submitReaction('${p.id}', 'LIKE')" title="Thích">👍</span>
              <span class="reaction-item" onclick="app.submitReaction('${p.id}', 'LOVE')" title="Yêu thích">❤️</span>
              <span class="reaction-item" onclick="app.submitReaction('${p.id}', 'HAHA')" title="Haha">😆</span>
              <span class="reaction-item" onclick="app.submitReaction('${p.id}', 'WOW')" title="Wow">😮</span>
              <span class="reaction-item" onclick="app.submitReaction('${p.id}', 'SAD')" title="Buồn">😢</span>
              <span class="reaction-item" onclick="app.submitReaction('${p.id}', 'ANGRY')" title="Phẫn nộ">😡</span>
            </div>
          </div>

          <!-- Nút Bình luận: Bấm vào mở Modal xem bình luận như Hình 2 -->
          <button class="btn-post-action" onclick="app.openCommentModal('${p.id}')">
            <span>💬</span>
            <span>Bình luận</span>
          </button>

          <!-- Nút Đăng lại bài viết của người khác lên trang cá nhân -->
          ${!isAuthor ? `
            <button class="btn-post-action btn-repost" onclick="app.openRepostModal('${p.id}')" title="Đăng lại bài viết này lên trang cá nhân">
              <span>🔁</span>
              <span>Đăng lại</span>
            </button>
          ` : ''}

          <!-- Nút Chia sẻ: Bấm vào mở Modal chia sẻ vào chat bạn bè/nhóm -->
          <button class="btn-post-action" onclick="app.openShareToChatModal('${p.id}')">
            <span>📨</span>
            <span>Chia sẻ</span>
          </button>
        </div>
      </article>
    `;
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
      imgPrev.src = this.editingPostMedia.url;
      imgPrev.style.display = 'block';
      vidPrev.style.display = 'none';
      container.style.display = 'block';
    } else if (this.editingPostMedia && this.editingPostMedia.type === 'VIDEO' && this.editingPostMedia.url) {
      vidPrev.src = this.editingPostMedia.url;
      vidPrev.style.display = 'block';
      imgPrev.style.display = 'none';
      container.style.display = 'block';
    } else {
      imgPrev.src = '';
      vidPrev.src = '';
      imgPrev.style.display = 'none';
      vidPrev.style.display = 'none';
      container.style.display = 'none';
    }
  }

  handleEditPostImageAttachment(input) {
    if (input.files && input.files[0]) {
      const file = input.files[0];
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

  handleEditPostVideoAttachment(input) {
    if (input.files && input.files[0]) {
      const file = input.files[0];
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

  handleImageAttachment(input) {
    if (input.files && input.files[0]) {
      const file = input.files[0];
      const reader = new FileReader();
      reader.onload = (e) => {
        this.currentMedia = { type: 'IMAGE', url: e.target.result };
        document.getElementById('postImagePreview').src = e.target.result;
        document.getElementById('postImagePreview').style.display = 'block';
        document.getElementById('postVideoPreview').style.display = 'none';
        document.getElementById('mediaPreviewContainer').style.display = 'block';
      };
      reader.readAsDataURL(file);
    }
  }

  handleVideoAttachment(input) {
    if (input.files && input.files[0]) {
      const file = input.files[0];
      const reader = new FileReader();
      reader.onload = (e) => {
        this.currentMedia = { type: 'VIDEO', url: e.target.result };
        document.getElementById('postVideoPreview').src = e.target.result;
        document.getElementById('postVideoPreview').style.display = 'block';
        document.getElementById('postImagePreview').style.display = 'none';
        document.getElementById('mediaPreviewContainer').style.display = 'block';
      };
      reader.readAsDataURL(file);
    }
  }

  clearPostMedia() {
    this.currentMedia = { type: 'NONE', url: '' };
    const container = document.getElementById('mediaPreviewContainer');
    if (container) container.style.display = 'none';
    const imgPrev = document.getElementById('postImagePreview');
    if (imgPrev) imgPrev.src = '';
    const vidPrev = document.getElementById('postVideoPreview');
    if (vidPrev) vidPrev.src = '';
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
        if (window.location.pathname.includes('profile.html')) {
          if (this.profileUserId) this.loadUserPosts(this.profileUserId);
        } else {
          this.loadFeed();
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
    const modal = document.getElementById('commentModal');
    if (!modal) return;

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
      if (post.media_type === 'IMAGE' && post.media_url) {
        mediaContainer.innerHTML = `<img src="${post.media_url}" style="margin-top: 0.5rem;" alt="Media">`;
      } else if (post.media_type === 'VIDEO' && post.media_url) {
        mediaContainer.innerHTML = `<video src="${post.media_url}" controls style="margin-top: 0.5rem;"></video>`;
      } else {
        mediaContainer.innerHTML = '';
      }
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
            if (r.type === 'LOVE') emoji = '❤️';
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
          <div class="reaction-cluster-box" onclick="app.openPostReactionsModal('${post.id}')" title="${this.escapeHtml(tt)}">
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
    if (commentsCount) commentsCount.textContent = (post.comments && Array.isArray(post.comments)) ? post.comments.length : 0;

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
                  <img class="c-avatar" src="${r.author_avatar || '/img/default-avatar.svg'}" onclick="app.openUserProfile('${r.author_id}')" style="cursor: pointer;">
                  <div class="comment-bubble">
                    <div class="c-author" onclick="app.openUserProfile('${r.author_id}')">${this.escapeHtml(r.author_name)}</div>
                    <div class="c-text">
                      ${r.reply_to_author ? `<span class="mention-user-tag" onclick="app.openUserProfile('${r.author_id}')">@${this.escapeHtml(r.reply_to_author)}</span>` : ''}
                      ${this.escapeHtml(r.content)}
                    </div>
                  </div>
                  <div class="comment-more-wrapper">
                    <button class="comment-more-btn" onclick="app.toggleCommentMenu(event, '${r.id}')" title="Tùy chọn">•••</button>
                    <div class="comment-more-dropdown" id="cmt-menu-${r.id}">
                      ${canDeleteReply ? `
                        <button class="post-dropdown-item text-danger" onclick="app.deleteComment('${r.id}')">
                          <span>🗑️</span> Xóa bình luận
                        </button>
                      ` : ''}
                      <button class="post-dropdown-item" onclick="app.openReportModal('', '', '${r.id}')">
                        <span>🚩</span> Báo cáo
                      </button>
                    </div>
                  </div>
                </div>
                <div class="comment-actions-bar">
                  <span>${replyTime}</span>
                  <button class="comment-action-btn" onclick="app.prepareReplyComment('${c.id}', '${this.escapeHtml(r.author_name)}')">Phản hồi</button>
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
          <img class="c-avatar" src="${c.author_avatar || '/img/default-avatar.svg'}" onclick="app.openUserProfile('${c.author_id}')" style="cursor: pointer;">
          <div class="comment-bubble">
            <div class="c-author" onclick="app.openUserProfile('${c.author_id}')">${this.escapeHtml(c.author_name)}</div>
            <div class="c-text">${this.escapeHtml(c.content)}</div>
          </div>
          <div class="comment-more-wrapper">
            <button class="comment-more-btn" onclick="app.toggleCommentMenu(event, '${c.id}')" title="Tùy chọn">•••</button>
            <div class="comment-more-dropdown" id="cmt-menu-${c.id}">
              ${canDelete ? `
                <button class="post-dropdown-item text-danger" onclick="app.deleteComment('${c.id}')">
                  <span>🗑️</span> Xóa bình luận
                </button>
              ` : ''}
              <button class="post-dropdown-item" onclick="app.openReportModal('', '', '${c.id}')">
                <span>🚩</span> Báo cáo
              </button>
            </div>
          </div>
        </div>
        <div class="comment-actions-bar">
          <span>${timeAgoStr}</span>
          <button class="comment-action-btn" onclick="app.prepareReplyComment('${c.id}', '${this.escapeHtml(c.author_name)}')">Phản hồi</button>
        </div>
        ${repliesHtml}
      </div>
    `;
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
            <div class="search-history-item" onclick="app.reSearchHistory('${safeQ}')">
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
        <div class="search-suggestion-item" onclick="app.saveSearchHistory('${encodeURIComponent(query)}'); app.openUserProfile('${u.user_token || u.id}')">
          <img src="${u.avatar || '/img/default-avatar.svg'}" alt="Avatar">
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
        <div class="search-suggestion-item" onclick="app.saveSearchHistory('${encodeURIComponent(query)}'); app.openGroupChat('${g.id}', '${this.escapeHtml(g.name)}', '${this.escapeHtml(g.avatar || '')}')">
          <img src="${g.avatar || '/img/default-avatar.svg'}" alt="Avatar">
          <div class="search-suggestion-info">
            <span class="ss-name">${this.escapeHtml(g.name)}</span>
            <span class="ss-sub">${this.escapeHtml(g.subtitle || '')}</span>
          </div>
        </div>
      `).join('');
    }

    html += `
      <div class="search-suggestion-more" onclick="app.navigateToSearchPage('${encodeURIComponent(query)}')">
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
              <h3 style="font-size: 1.05rem; margin-bottom: 0.8rem;">Mọi người</h3>
              <div class="friends-grid">
                ${users.map(u => `
                  <div class="friend-card">
                    <img class="fc-avatar" src="${u.avatar || '/img/default-avatar.svg'}" onclick="app.openUserProfile('${u.user_token || u.id}')">
                    <div class="friend-card-content">
                      <h4 onclick="app.openUserProfile('${u.user_token || u.id}')">${this.escapeHtml(u.name)}</h4>
                      <div class="fc-sub">${this.escapeHtml(u.subtitle || ('@' + u.username))}</div>
                      <div class="friend-card-actions">
                        <button class="btn-primary" style="font-size: 0.8rem;" onclick="app.openUserProfile('${u.user_token || u.id}')">Xem trang cá nhân</button>
                        <button class="btn-secondary" style="font-size: 0.8rem; color: var(--accent-red);" onclick="app.openReportModal('${u.id}', '')" title="Báo cáo người dùng">🚩 Báo cáo</button>
                      </div>
                    </div>
                  </div>
                `).join('')}
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
                    <img class="fc-avatar" src="${g.avatar || '/img/default-avatar.svg'}" style="height: 120px;">
                    <div class="friend-card-content">
                      <h4>${this.escapeHtml(g.name)}</h4>
                      <div class="fc-sub">${this.escapeHtml(g.subtitle || '')}</div>
                      <p style="font-size: 0.8rem; color: var(--text-muted);">${this.escapeHtml(g.description || '')}</p>
                      <div class="friend-card-actions">
                        <button class="btn-primary" style="font-size: 0.8rem;" onclick="app.openGroupChat('${g.id}', '${this.escapeHtml(g.name)}', '${this.escapeHtml(g.avatar || '')}')">💬 Vào nhắn tin</button>
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
              <div style="display: flex; flex-direction: column; gap: 0.8rem;">
                ${posts.map(p => `
                  <div style="background: var(--bg-surface-secondary); padding: 0.9rem; border-radius: 8px; cursor: pointer;" onclick="app.openCommentModal('${p.id}')">
                    <div style="display: flex; align-items: center; gap: 0.5rem; margin-bottom: 0.4rem;">
                      <img src="${p.author_avatar || '/img/default-avatar.svg'}" style="width: 32px; height: 32px; border-radius: 50%;">
                      <div>
                        <strong>${this.escapeHtml(p.author_name)}</strong>
                        <div style="font-size: 0.75rem; color: var(--text-muted);">${this.formatTimeAgo(p.created_at)}</div>
                      </div>
                    </div>
                    <div style="font-size: 0.9rem;">${this.escapeHtml(p.content)}</div>
                  </div>
                `).join('')}
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
  async loadNotifications() {
    if (!this.currentUser) return;
    try {
      const res = await fetch(`/api/notifications?user_id=${this.currentUser.id}`);
      const data = await res.json();

      if (data.status === 'ok') {
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
          if (!data.notifications || data.notifications.length === 0) {
            list.innerHTML = '<div style="padding: 1.5rem; text-align: center; color: var(--text-muted); font-size: 0.85rem;">Không có thông báo mới</div>';
          } else {
            list.innerHTML = data.notifications.map(n => `
              <div class="notif-item ${n.is_read ? '' : 'unread'}" id="notif-item-${n.id}" onclick="app.handleNotificationClick('${n.link}', ${n.id})">
                <img src="${n.actor_avatar || '/img/default-avatar.svg'}" alt="Avatar">
                <div class="notif-text">
                  <strong>${this.escapeHtml(n.actor_name)}</strong> ${this.escapeHtml(n.content)}
                  <div class="notif-time">${this.formatTimeAgo(n.created_at)}</div>
                </div>
                ${!n.is_read ? '<span class="unread-blue-dot notif-dot" title="Thông báo chưa đọc"></span>' : ''}
              </div>
            `).join('');
          }
        }
      }
    } catch (e) {
      console.error(e);
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
    } catch (e) {}
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
      } catch (e) {}
    }

    if (!link) return;
    if (link.startsWith('/#post-')) {
      const postId = link.replace('/#post-', '');
      this.openCommentModal(postId);
    } else {
      window.location.href = link;
    }
  }

  // =========================================================================
  // 9. GỢI Ý BẠN BÈ & BỘ LỌC GỢI Ý
  // =========================================================================
  async filterSuggestionsTab(filter, btn) {
    this.currentSuggestionFilter = filter;
    document.querySelectorAll('.suggestion-filter-pill').forEach(b => b.classList.remove('active'));
    if (btn) btn.classList.add('active');
    this.currentSuggestionLimit = 5;
    this.loadSuggestions();
  }

  async loadMoreSuggestionsFeed() {
    this.currentSuggestionLimit += 5;
    await this.loadSuggestions();
  }

  async loadMoreSuggestions() {
    await this.loadMoreSuggestionsFeed();
  }

  async loadSuggestions() {
    const container = document.getElementById('suggestionsContainer') || document.getElementById('suggestionsGrid');
    if (!container || !this.currentUser) return;
    const moreBtn = document.getElementById('btnLoadMoreSuggestionsFeed') || document.getElementById('btnLoadMoreSuggestions');

    try {
      const res = await fetch(`/api/friends/suggest?user_id=${this.currentUser.id}&filter=${this.currentSuggestionFilter}&limit=${this.currentSuggestionLimit}`);
      const data = await res.json();

      if (data.status === 'ok') {
        if (!data.suggestions || data.suggestions.length === 0) {
          container.innerHTML = '<div style="font-size: 0.85rem; color: var(--text-muted); padding: 1rem; text-align: center;">Chưa tìm thấy gợi ý kết bạn phù hợp với bộ lọc này.</div>';
          if (moreBtn) moreBtn.style.display = 'none';
          return;
        }

        if (moreBtn) {
          moreBtn.style.display = (data.suggestions.length >= this.currentSuggestionLimit) ? 'inline-block' : 'none';
        }

        container.innerHTML = data.suggestions.map(s => {
          const sugTarget = s.user_token || s.id;
          let btnHtml = '';
          if (s.request_status === 'PENDING_SENT') {
            btnHtml = `<button class="btn-secondary btn-pending-status" style="font-size: 0.8rem;" onclick="app.handleCancelFriendRequest('${s.id}', '${this.escapeHtml(s.name)}')"><span class="btn-pending-label">⏳ Đã gửi lời mời</span><span class="btn-cancel-request-label">✕ Hủy gửi lời mời</span></button>`;
          } else if (s.request_status === 'PENDING_RECEIVED') {
            btnHtml = `<button class="btn-primary" style="font-size: 0.8rem;" onclick="app.respondFriendRequest('${s.id}', 'ACCEPT')">Chấp nhận</button>`;
          } else if (s.request_status === 'FRIEND') {
            btnHtml = `<span style="font-size: 0.8rem; color: var(--accent-green);">✓ Bạn bè</span>`;
          } else {
            btnHtml = `<button class="btn-add-friend" onclick="app.sendFriendRequest('${s.id}', '${this.escapeHtml(s.name)}')">+ Kết bạn</button>`;
          }

          return `
            <div class="suggestion-item" id="sug-${s.id}">
              <div class="suggestion-user-info" onclick="app.openUserProfile('${sugTarget}')" style="cursor: pointer;">
                <img src="${s.avatar || '/img/default-avatar.svg'}" alt="Avatar">
                <div class="suggestion-user-details">
                  <h4>${this.escapeHtml(s.name)}</h4>
                  <div class="suggestion-reason">${s.reason}</div>
                  ${s.mutual > 0 ? `<div class="suggestion-mutual">${s.mutual} bạn chung</div>` : ''}
                </div>
              </div>
              <div id="sug-btn-box-${s.id}">${btnHtml}</div>
            </div>
          `;
        }).join('');
      }
    } catch (err) {
      console.error(err);
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
        // Lấy tên: ưu tiên tham số truyền vào, sau đó lấy từ suggestion item, cuối cùng là profFullname
        const resolvedName = targetName
          || document.querySelector(`#sug-${targetId} h4`)?.textContent
          || document.getElementById('profFullname')?.textContent
          || targetId;
        const box = document.getElementById(`sug-btn-box-${targetId}`);
        if (box) {
          box.innerHTML = `<button class="btn-secondary btn-pending-status" style="font-size:0.8rem;" onclick="app.handleCancelFriendRequest('${targetId}', '${this.escapeHtml(resolvedName)}')"><span class="btn-pending-label">⏳ Đã gửi lời mời</span><span class="btn-cancel-request-label">✕ Hủy gửi lời mời</span></button>`;
        }
        const profBtn = document.getElementById('profActionFriendBtn');
        if (profBtn) {
          profBtn.outerHTML = `
            <button class="btn-secondary btn-pending-status" id="profFriendStatusBtn" onclick="app.handleCancelFriendRequest('${targetId}', '${this.escapeHtml(resolvedName)}')">
              <span class="btn-pending-label">⏳ Đã gửi lời mời</span>
              <span class="btn-cancel-request-label">✕ Hủy gửi lời mời</span>
            </button>
          `;
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
        this.loadSuggestions();
        this.loadFriendsAndContacts();
        if (window.location.pathname.includes('friends.html')) {
          this.initFriendsPage();
        }
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
    const uid = this.currentUser.id;
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
      userId = this.currentUser.id;
    }
    if (!userId) {
      const stored = localStorage.getItem('cs_user');
      if (stored) {
        try {
          const parsed = JSON.parse(stored);
          userId = parsed.id;
        } catch(e) {}
      }
    }
    if (!userId) return;

    try {
      const res = await fetch(`/api/users/profile?id=${encodeURIComponent(userId)}&my_id=${this.currentUser ? this.currentUser.id : ''}`);
      const data = await res.json();

      if (data.status === 'ok') {
        const u = data.user;
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
            `;
          } else {
            let friendBtn = '';
            if (data.relation === 'FRIEND') {
              friendBtn = `
                <button class="btn-secondary btn-friend-status" id="profFriendStatusBtn" onclick="app.handleUnfriend('${u.id}', '${this.escapeHtml(u.fullname)}')">
                  <span class="btn-friend-label">✓ Bạn bè</span>
                  <span class="btn-unfriend-label">✕ Xóa bạn</span>
                </button>
              `;
            } else if (data.relation === 'PENDING_SENT') {
              friendBtn = `
                <button class="btn-secondary btn-pending-status" id="profFriendStatusBtn" onclick="app.handleCancelFriendRequest('${u.id}', '${this.escapeHtml(u.fullname)}')">
                  <span class="btn-pending-label">⏳ Đã gửi lời mời</span>
                  <span class="btn-cancel-request-label">✕ Hủy gửi lời mời</span>
                </button>
              `;
            } else if (data.relation === 'PENDING_RECEIVED') {
              friendBtn = `<button class="btn-primary" onclick="app.respondFriendRequest('${u.id}', 'ACCEPT')">✔️ Chấp nhận kết bạn</button>`;
            } else {
              friendBtn = `<button class="btn-primary" id="profActionFriendBtn" onclick="app.sendFriendRequest('${u.id}')">+ Thêm bạn bè</button>`;
            }

            actionRow.innerHTML = `
              ${friendBtn}
              <button class="btn-secondary" onclick="app.openChatWith('${u.id}', '${this.escapeHtml(u.fullname)}', '${u.avatar}')">💬 Nhắn tin</button>
              <button class="btn-secondary" onclick="app.openReportModal('${u.id}', '')" title="Báo cáo người dùng">🚩</button>
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
      this.isLoadingProfile = false;
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

        if (previewGrid) {
          previewGrid.innerHTML = data.friends.slice(0, 6).map(f => `
            <div style="cursor: pointer; text-align: center;" onclick="app.openUserProfile('${f.id}')">
              <img src="${f.avatar || '/img/default-avatar.svg'}" style="width: 100%; height: 80px; object-fit: cover; border-radius: 8px;">
              <div style="font-size: 0.75rem; font-weight: 600; margin-top: 0.2rem; white-space: nowrap; overflow: hidden; text-overflow: ellipsis;">${this.escapeHtml(f.fullname)}</div>
            </div>
          `).join('');
        }

        if (fullGrid) {
          fullGrid.innerHTML = data.friends.map(f => `
            <div class="friend-card">
              <img class="fc-avatar" src="${f.avatar || '/img/default-avatar.svg'}" onclick="app.openUserProfile('${f.id}')">
              <div class="friend-card-content">
                <h4 onclick="app.openUserProfile('${f.id}')">${this.escapeHtml(f.fullname)}</h4>
                <div class="fc-sub">${this.escapeHtml(f.school || f.hometown || ('@' + f.username))}</div>
                <div class="friend-card-actions">
                  <button class="btn-primary" style="font-size: 0.8rem;" onclick="app.openUserProfile('${f.id}')">Xem trang cá nhân</button>
                </div>
              </div>
            </div>
          `).join('');
        }
      }
    } catch (err) {
      console.error(err);
    }
  }

  // =========================================================================
  // 11. TRANG BẠN BÈ RIÊNG BIỆT (friends.html)
  // =========================================================================
  initFriendsPage(initialTab = 'all') {
    this.switchFriendsPageTab(initialTab);
    this.loadAllFriendsList();
    this.loadPendingFriendRequests();
    this.loadSuggestions();
  }

  switchFriendsPageTab(tab) {
    ['tabAllFriendsBtn', 'tabRequestsBtn', 'tabSuggestionsBtn'].forEach(id => {
      document.getElementById(id)?.classList.remove('active');
    });
    ['tabContentAllFriends', 'tabContentRequests', 'tabContentSuggestions'].forEach(id => {
      const el = document.getElementById(id);
      if (el) el.style.display = 'none';
    });

    if (tab === 'all') {
      document.getElementById('tabAllFriendsBtn')?.classList.add('active');
      document.getElementById('tabContentAllFriends').style.display = 'block';
    } else if (tab === 'requests') {
      document.getElementById('tabRequestsBtn')?.classList.add('active');
      document.getElementById('tabContentRequests').style.display = 'block';
    } else if (tab === 'suggestions') {
      document.getElementById('tabSuggestionsBtn')?.classList.add('active');
      document.getElementById('tabContentSuggestions').style.display = 'block';
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
        const countBadge = document.getElementById('allFriendsCount');
        if (countBadge) countBadge.textContent = this.fullFriendsCache.length;

        this.renderFriendsCards(this.fullFriendsCache);
      }
    } catch (e) {
      console.error(e);
    }
  }

  renderFriendsCards(friends) {
    const grid = document.getElementById('allFriendsGrid');
    if (!grid) return;

    if (!friends || friends.length === 0) {
      grid.innerHTML = '<div style="text-align: center; padding: 2rem; color: var(--text-muted); grid-column: 1 / -1;">Bạn chưa kết bạn với ai. Hãy khám phá phần Gợi ý bạn bè!</div>';
      return;
    }

    grid.innerHTML = friends.map(f => `
      <div class="friend-card" id="friend-card-${f.id}">
        <!-- Nhấn vào avatar để sang web profile của bạn bè -->
        <img class="fc-avatar" src="${f.avatar || '/img/default-avatar.svg'}" onclick="app.openUserProfile('${f.id}')" title="Nhấn để xem trang cá nhân">
        <div class="friend-card-content">
          <h4 onclick="app.openUserProfile('${f.id}')">${this.escapeHtml(f.fullname)}</h4>
          <div class="fc-sub">${this.escapeHtml(f.school || f.hometown || ('@' + f.username))}</div>
          <div class="friend-card-actions">
            <button class="btn-primary" style="font-size: 0.8rem;" onclick="app.openChatWith('${f.id}', '${this.escapeHtml(f.fullname)}', '${f.avatar}')">💬 Nhắn tin</button>
            <button class="btn-secondary" style="font-size: 0.8rem;" onclick="app.openUserProfile('${f.id}')">Xem trang cá nhân</button>
          </div>
        </div>
      </div>
    `).join('');
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
        const badge = document.getElementById('reqCountBadge');
        if (badge) badge.textContent = data.requests.length;

        if (data.requests.length === 0) {
          grid.innerHTML = '<div style="text-align: center; padding: 2rem; color: var(--text-muted); grid-column: 1 / -1;">Không có lời mời kết bạn nào đang chờ.</div>';
          return;
        }

        grid.innerHTML = data.requests.map(u => `
          <div class="friend-card">
            <img class="fc-avatar" src="${u.avatar || '/img/default-avatar.svg'}" onclick="app.openUserProfile('${u.id}')">
            <div class="friend-card-content">
              <h4 onclick="app.openUserProfile('${u.id}')">${this.escapeHtml(u.fullname)}</h4>
              <div class="fc-sub">${this.escapeHtml(u.school || u.hometown || '')}</div>
              <div class="friend-card-actions">
                <button class="btn-primary" style="font-size: 0.8rem;" onclick="app.respondFriendRequest('${u.id}', 'ACCEPT')">Chấp nhận</button>
                <button class="btn-secondary" style="font-size: 0.8rem;" onclick="app.respondFriendRequest('${u.id}', 'REJECT')">Xóa lời mời</button>
              </div>
            </div>
          </div>
        `).join('');
      }
    } catch (e) {
      console.error(e);
    }
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
              <img src="${display.author_avatar || post.author_avatar || '/img/default-avatar.svg'}" style="width:32px;height:32px;border-radius:50%;object-fit:cover;" alt="">
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
        last_message: g.last_message || g.description || 'Nhóm trò chuyện',
        last_message_time: g.last_message_time || g.created_at || '',
        is_online: true,
        is_unread: false,
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
    const pills = document.querySelectorAll('.chat-filter-pill');
    pills.forEach(p => p.classList.remove('active'));
    if (btn) btn.classList.add('active');
    this.renderContactsList(filter);
  }

  renderContactsList(filter = 'all') {
    const container = document.getElementById('contactsContainer');
    if (!container || !this.allContactsCache) return;

    let items = [];
    if (filter === 'all') {
      items = [...this.allContactsCache.friends, ...this.allContactsCache.groups];
    } else if (filter === 'friends') {
      items = this.allContactsCache.friends;
    } else if (filter === 'unread') {
      items = this.allContactsCache.friends.filter(c => c.is_unread);
    } else if (filter === 'groups') {
      items = this.allContactsCache.groups;
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
      container.innerHTML = `<div style="font-size: 0.8rem; color: var(--text-muted); padding: 0.75rem; text-align: center;">${emptyMsg}</div>`;
      return;
    }

    // Cắt ngắn tin nhắn dài bằng dấu ... (Item 12)
    const truncate = (str, len = 24) => {
      if (!str) return '';
      return str.length > len ? str.substring(0, len) + '...' : str;
    };

    container.innerHTML = items.map(c => {
      if (c.itemType === 'group') {
        let groupLastMsg = c.last_message || 'Nhóm chat';
        if (typeof groupLastMsg === 'string') {
          if (groupLastMsg.startsWith('__SYS_RENAME__:')) {
            const p = groupLastMsg.split(':');
            const newName = p.slice(2).join(':') || '';
            groupLastMsg = newName ? `Đã đổi tên nhóm thành "${newName}"` : 'Đã đổi tên nhóm';
          }
        }
        return `
          <div class="contact-item" onclick="app.openGroupChat('${c.id}', '${this.escapeHtml(c.name)}', '${this.escapeHtml(c.avatar || '')}')">
            <div class="contact-avatar-box">
              <img src="${c.avatar || '/img/default-avatar.svg'}" alt="Group" class="group-avatar-clickable"
                   onclick="app.openImageLightbox('${(c.avatar || '/img/default-avatar.svg').replace(/'/g, "\\'")}', event)" title="Xem ảnh nhóm">
            </div>
            <div class="contact-info">
              <div class="contact-name">👥 ${this.escapeHtml(c.name)}</div>
              <div class="contact-last-msg">${this.escapeHtml(truncate(groupLastMsg))}</div>
            </div>
          </div>
        `;
      } else {
        const isMine = c.last_message_is_mine;
        const isUnread = c.is_unread;
        let prefix = isMine ? 'Bạn: ' : '';
        let statusNote = '';
        if (isMine) {
          statusNote = c.is_seen_by_partner ? ' · Đã xem' : ' · Đã gửi';
        }

        // Quyết định class hiển thị:
        // - Nếu là tin nhắn chưa đọc (đối phương gửi đến mà mình chưa đọc): hiện màu đen đậm (unread-msg, unread-name) và dấu chấm màu xanh dương!
        // - Nếu đã xem (hoặc tin nhắn cũ đã đọc): hiện chữ nhạt hơn (read-msg)!
        const displayName = c.nickname || c.name;
        let lastMsgText = c.last_message || 'Chưa có tin nhắn';
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

        return `
          <div class="contact-item ${isUnread ? 'has-unread' : ''}" onclick="app.openChatWith('${c.id}', '${this.escapeHtml(displayName)}', '${c.avatar}', '${c.user_token || c.id}')">
            <div class="contact-avatar-box">
              <img src="${c.avatar || '/img/default-avatar.svg'}" alt="Avatar">
              <span class="online-indicator ${c.is_online ? 'online' : 'offline'}"></span>
            </div>
            <div class="contact-info">
              <div class="${nameClass}">
                <span>${this.escapeHtml(displayName)}</span>
                ${blueDotHtml}
              </div>
              <div class="${msgClass}">
                ${prefix}${this.escapeHtml(truncate(lastMsgText))}${statusNote}
              </div>
            </div>
          </div>
        `;
      }
    }).join('');
  }

  async openChatWith(partnerId, partnerName, partnerAvatar, partnerToken = '') {
    this.currentChatPartner = { id: partnerId, name: partnerName, avatar: partnerAvatar, user_token: partnerToken || partnerId, isGroup: false };
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

    this.loadInitialMessages();
    // Bắt đầu typing indicator, read receipt & message polling theo thời gian thực
    this.startTypingIndicator();
    this.startTypingPoll();
    this.startReadReceiptPoll();
    this.startMessagePolling();
  }

  async openGroupChat(groupId, groupName, groupAvatar = '') {
    const gAvatar = groupAvatar || '/img/default-avatar.svg';
    this.currentChatPartner = { id: groupId, name: groupName, avatar: gAvatar, isGroup: true, role: 'MEMBER' };
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

    this.loadGroupMessages(groupId);
    this.startMessagePolling();
  }

  closeChat() {
    document.getElementById('chatWindow').style.display = 'none';
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
    if (diffSec < 0 || diffSec <= 300) return 'Đang hoạt động';
    const diffMin = Math.floor(diffSec / 60);
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

      const isGroup = this.currentChatPartner.isGroup;
      const url = isGroup
        ? `/api/groups/messages?group_id=${encodeURIComponent(this.currentChatPartner.id)}&viewer_id=${this.currentUser ? this.currentUser.id : ''}&limit=30`
        : `/api/messages?my_id=${this.currentUser.id}&partner_id=${this.currentChatPartner.id}&limit=30&before_id=0`;

      try {
        const res = await fetch(url);
        const data = await res.json();
        if (data.status === 'ok' && Array.isArray(data.messages)) {
          const signature = data.messages.map(m => `${m.id}_${m.is_revoked}_${m.reaction || ''}_${m.content}_${m.is_read || 0}`).join('|');
          if (signature !== this.lastRenderedMsgSignature) {
            this.lastRenderedMsgSignature = signature;
            const listEl = document.getElementById('messagesList');
            const chatBody = document.getElementById('chatBody');
            const isNearBottom = chatBody ? (chatBody.scrollHeight - chatBody.scrollTop - chatBody.clientHeight < 160) : true;

            listEl.innerHTML = data.messages.map((m, i) => this.renderMessageItem(m, data.messages[i - 1] || null, data.messages[i + 1] || null)).join('');

            if (isNearBottom && chatBody) {
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
      <span style="font-size:0.8rem; color:var(--text-primary); flex:1; overflow:hidden; text-overflow:ellipsis; white-space:nowrap;">${this.pendingChatMedia.name || 'Tệp đính kèm'}</span>
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
    } else {
      this.searchChatMessages('');
    }
  }

  searchChatMessages(term) {
    clearTimeout(this._chatSearchDebounce);
    const q = (term || '').trim();
    const dropdown = document.getElementById('chatSearchResultsDropdown');
    if (!dropdown) return;

    // Hiện lại tất cả tin nhắn đang hiển thị (bỏ filter client-side cũ)
    document.querySelectorAll('#messagesList .message-row').forEach(r => { r.style.display = 'flex'; });

    if (!q) {
      dropdown.style.display = 'none';
      dropdown.innerHTML = '';
      return;
    }
    if (!this.currentChatPartner || !this.currentUser) {
      dropdown.style.display = 'none';
      return;
    }

    this._chatSearchDebounce = setTimeout(async () => {
      try {
        const isGroup = !!this.currentChatPartner.isGroup;
        const url = `/api/messages/search?my_id=${encodeURIComponent(this.currentUser.id)}&partner_id=${encodeURIComponent(this.currentChatPartner.id)}&q=${encodeURIComponent(q)}&is_group=${isGroup ? 'true' : 'false'}`;
        const res = await fetch(url);
        const data = await res.json();
        if (data.status !== 'ok') {
          dropdown.innerHTML = `<div class="chat-search-item" style="color:var(--text-muted);">Lỗi tìm kiếm</div>`;
          dropdown.style.display = 'block';
          return;
        }
        const msgs = data.messages || [];
        if (msgs.length === 0) {
          dropdown.innerHTML = `<div class="chat-search-item" style="color:var(--text-muted); text-align:center;">Không tìm thấy tin nhắn phù hợp</div>`;
          dropdown.style.display = 'block';
          return;
        }
        const esc = (s) => this.escapeHtml(s || '');
        const highlightSnippet = (content, query) => {
          const raw = content || '';
          const lowerRaw = this.removeVietnameseTonesClient(raw);
          const lowerQ = this.removeVietnameseTonesClient(query);
          const idx = lowerRaw.indexOf(lowerQ);
          if (idx < 0) return esc(raw.length > 80 ? raw.slice(0, 80) + '…' : raw);
          // Map approximate index back (best-effort for display)
          let start = Math.max(0, idx - 20);
          let end = Math.min(raw.length, idx + query.length + 40);
          let snippet = raw.slice(start, end);
          if (start > 0) snippet = '…' + snippet;
          if (end < raw.length) snippet = snippet + '…';
          // Simple highlight by re-finding case-insensitive in snippet
          try {
            const re = new RegExp('(' + query.replace(/[.*+?^${}()|[\]\\]/g, '\\$&') + ')', 'gi');
            snippet = snippet.replace(re, '<span class="chat-search-match-highlight">$1</span>');
          } catch (_) {}
          return snippet;
        };

        dropdown.innerHTML = msgs.map(m => {
          const sender = esc(m.sender_name || 'Người dùng');
          const time = this.formatTimeAgo(m.created_at);
          const snippet = highlightSnippet(m.content, q);
          return `
            <div class="chat-search-item" data-msg-id="${m.id}"
                 oncontextmenu="app.onChatSearchResultContextMenu(event, ${m.id}); return false;"
                 onclick="app.jumpToChatMessage(${m.id}, event)">
              <div class="chat-search-item-header">
                <span class="chat-search-item-sender">${sender}</span>
                <span>${time}</span>
              </div>
              <div class="chat-search-item-snippet">${snippet}</div>
              <div class="chat-search-item-hint">🖱️ Chuột phải / trái: chuyển tới tin nhắn</div>
            </div>`;
        }).join('');
        dropdown.style.display = 'block';
      } catch (err) {
        console.error(err);
        dropdown.innerHTML = `<div class="chat-search-item" style="color:var(--text-muted);">Lỗi kết nối</div>`;
        dropdown.style.display = 'block';
      }
    }, 250);
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
    if (!msgId || !this.currentChatPartner || !this.currentUser) return;

    // Ẩn dropdown tìm kiếm
    const dropdown = document.getElementById('chatSearchResultsDropdown');
    if (dropdown) dropdown.style.display = 'none';

    const listEl = document.getElementById('messagesList');
    const chatBody = document.getElementById('chatBody');
    if (!listEl || !chatBody) return;

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
          listEl.innerHTML = msgs.map((m, i) => this.renderMessageItem(m, msgs[i - 1] || null, msgs[i + 1] || null)).join('');
          this.lastRenderedMsgSignature = msgs.map(m => `${m.id}_${m.is_revoked}_${m.reaction || ''}_${m.content}_${m.is_read || 0}`).join('|');
          if (msgs.length > 0) {
            this.oldestMessageId = msgs[0].id;
            this.hasMoreMessages = true;
          }
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

    // Căn giữa tin nhắn trong khung chat
    const bodyRect = chatBody.getBoundingClientRect();
    const elRect = targetEl.getBoundingClientRect();
    const offset = (elRect.top + elRect.height / 2) - (bodyRect.top + bodyRect.height / 2);
    chatBody.scrollTop += offset;

    // Làm nổi bật trong giây lát
    targetEl.classList.remove('msg-highlight-pulse');
    // Force reflow
    void targetEl.offsetWidth;
    targetEl.classList.add('msg-highlight-pulse');
    setTimeout(() => {
      targetEl.classList.remove('msg-highlight-pulse');
    }, 2600);
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
            actionBtn = `<button class="btn-secondary" style="color: var(--accent-red); font-size: 0.75rem; padding: 2px 8px;" onclick="app.removeMemberFromGroup('${m.id}')">Xóa</button>`;
          }
          return `
            <div style="display: flex; align-items: center; justify-content: space-between; padding: 0.5rem 0; border-bottom: 1px solid var(--border-color);">
              <div style="display: flex; align-items: center; gap: 0.6rem; cursor: pointer;" onclick="app.openUserProfile('${m.user_token || m.id}')">
                <img src="${m.avatar || '/img/default-avatar.svg'}" style="width: 38px; height: 38px; border-radius: 50%; object-fit: cover;">
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
      if (data.status === 'ok') {
        const listEl = document.getElementById('addGroupMemberList');
        if (data.friends.length === 0) {
          listEl.innerHTML = '<div style="text-align: center; color: var(--text-muted); padding: 1.5rem; font-size: 0.85rem;">Tất cả bạn bè của bạn đã có mặt trong nhóm!</div>';
        } else {
          listEl.innerHTML = data.friends.map(f => `
            <div style="display: flex; align-items: center; justify-content: space-between; padding: 0.5rem 0; border-bottom: 1px solid var(--border-color);">
              <div style="display: flex; align-items: center; gap: 0.6rem;">
                <img src="${f.avatar || '/img/default-avatar.svg'}" style="width: 38px; height: 38px; border-radius: 50%; object-fit: cover;">
                <div style="font-weight: 600; font-size: 0.88rem;">${this.escapeHtml(f.name)}</div>
              </div>
              <button class="btn-primary" style="font-size: 0.8rem; padding: 3px 10px;" onclick="app.submitAddMember('${f.id}')">+ Thêm</button>
            </div>
          `).join('');
        }
        document.getElementById('addGroupMemberModal').style.display = 'flex';
      }
    } catch (e) {}
  }

  async submitAddMember(userId) {
    if (!this.currentChatPartner) return;
    try {
      const res = await fetch('/api/groups/members/add', {
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
        this.showToast('Đã thêm thành viên vào nhóm!', 'success');
        document.getElementById('addGroupMemberModal').style.display = 'none';
        this.loadGroupMessages(this.currentChatPartner.id);
      } else {
        this.showToast(data.message || 'Lỗi thêm thành viên', 'error');
      }
    } catch (e) {}
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
        this.lastRenderedMsgSignature = data.messages.map(m => `${m.id}_${m.is_revoked}_${m.reaction || ''}_${m.content}`).join('|');
        listEl.innerHTML = data.messages.map((m, i) => this.renderMessageItem(m, data.messages[i - 1] || null, data.messages[i + 1] || null)).join('');

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
    if (m.reply_to_text) {
      const origId = m.reply_to_id || 0;
      html += `<div class="chat-reply-quote" onclick="app.jumpToChatMessage(${origId}, event)" title="Nhấn chuột trái để chuyển tới tin nhắn gốc">↪️ ${this.escapeHtml(m.reply_to_text)}</div>`;
    }
    if (m.content) {
      html += `<div class="message-text">${this.escapeHtml(m.content)}</div>`;
    }

    // Media đính kèm (Hình ảnh & Video) (Item 1)
    if (m.media_url) {
      if (m.media_type === 'IMAGE') {
        html += `<div class="chat-media-wrapper"><img src="${m.media_url}" class="chat-media-img" onclick="window.open('${m.media_url}')" alt="Ảnh"></div>`;
      } else if (m.media_type === 'VIDEO') {
        html += `<div class="chat-media-wrapper"><video src="${m.media_url}" controls class="chat-media-video"></video></div>`;
      }
    }

    if (m.shared_post_id) {
      const uniqueCardId = `shared-post-${m.id || Math.floor(Math.random()*1000000)}`;
      html += `
        <div class="shared-post-bubble-card" id="${uniqueCardId}" onclick="app.viewSharedPost('${m.shared_post_id}')">
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
          if (p.media_type === 'IMAGE') {
            mediaHtml = `<div class="shared-post-bubble-media"><img src="${p.media_url}" alt="Post media"></div>`;
          } else if (p.media_type === 'VIDEO') {
            mediaHtml = `<div class="shared-post-bubble-media"><video src="${p.media_url}"></video></div>`;
          }
        }
        el.innerHTML = `
          <div class="shared-post-bubble-header">
            <img src="${p.author_avatar || '/img/default-avatar.svg'}" alt="Avatar">
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

        this.lastRenderedMsgSignature = data.messages.map(m => `${m.id}_${m.is_revoked}_${m.reaction || ''}_${m.content}`).join('|');
        listEl.innerHTML = data.messages.map((m, i) => this.renderMessageItem(m, data.messages[i - 1] || null, data.messages[i + 1] || null)).join('');

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
        const olderHtml = data.messages.map((m, i) => this.renderMessageItem(m, data.messages[i - 1] || null, data.messages[i + 1] || null)).join('');
        listEl.insertAdjacentHTML('afterbegin', olderHtml);
        chatBody.scrollTop = chatBody.scrollHeight - prevScrollHeight;
      } else {
        this.hasMoreMessages = false;
      }
    } catch (e) {
      console.error(e);
    } finally {
      this.isLoadingOlderMessages = false;
    }
  }

  renderMessageItem(m, prevMsg = null, nextMsg = null) {
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
        avatarHtml = `<img class="chat-avatar-mini" src="${m.sender_avatar || '/img/default-avatar.svg'}" alt="Avatar">`;
      } else {
        avatarHtml = `<div class="chat-avatar-spacer"></div>`;
      }
    }

    // Thanh 3 nút tròn khi lia chuột (Hình 2)
    let actionsHtml = '';
    if (!isRecalled) {
      const moreBtn = `
        <div style="position: relative;">
          <button class="msg-circle-btn" onclick="app.toggleMessageMoreMenu(${m.id}, event)" title="Thêm">⋮</button>
          <div class="msg-more-dropdown" id="msg-more-${m.id}">
            ${isMe ? `
              <button class="msg-dropdown-item" onclick="app.handleRecallClick(${m.id}, '${m.created_at || ''}')">↩️ Thu hồi</button>
              <button class="msg-dropdown-item danger" onclick="app.deleteMessage(${m.id})">🗑️ Xóa</button>
            ` : `
              <button class="msg-dropdown-item danger" onclick="app.deleteMessage(${m.id})">🗑️ Xóa</button>
              <button class="msg-dropdown-item" onclick="app.reportMessage(${m.id}, '${m.sender_id}')">🚩 Báo cáo</button>
            `}
          </div>
        </div>
      `;

      const replyBtn = `
        <button class="msg-circle-btn" onclick="app.startReplyToMessage(${m.id}, '${this.escapeHtml(m.sender_name || (isMe ? 'Bạn' : 'Bạn bè'))}', '${this.escapeHtml(m.content || '')}')" title="Trả lời">
          ↩️
        </button>
      `;

      const reactBtn = `
        <div style="position: relative;">
          <button class="msg-circle-btn" onclick="app.toggleMessageReactionPicker(${m.id}, event)" title="Thả icon">☺️</button>
          <div class="msg-reaction-picker" id="msg-react-picker-${m.id}">
            <span onclick="app.reactToChatMessage(${m.id}, '❤️')">❤️</span>
            <span onclick="app.reactToChatMessage(${m.id}, '👍')">👍</span>
            <span onclick="app.reactToChatMessage(${m.id}, '😆')">😆</span>
            <span onclick="app.reactToChatMessage(${m.id}, '😮')">😮</span>
            <span onclick="app.reactToChatMessage(${m.id}, '😢')">😢</span>
            <span onclick="app.reactToChatMessage(${m.id}, '😡')">😡</span>
          </div>
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

    // Thời gian hiển thị
    let showTime = (isLast || isSingle);
    const timeStr = m.created_at ? m.created_at.substring(11, 16) : '';
    const timeHtml = `<div class="message-time${showTime ? '' : ' msg-time-hidden'}" data-time="${timeStr}" onclick="this.classList.toggle('msg-time-visible');">${timeStr}</div>`;

    // Reaction badge ở góc dưới bubble
    const reactionBadgeHtml = m.reaction ? `<div class="msg-reaction-badge" title="Cảm xúc">${m.reaction}</div>` : '';

    const bubbleHtml = `
      <div class="message-bubble" onclick="const t = this.closest('.message-content-wrapper').querySelector('.msg-time-hidden'); if (t) t.classList.toggle('msg-time-visible');">
        ${this.renderMessageContent(m)}
        ${reactionBadgeHtml}
      </div>
    `;

    return `
      ${senderHeaderHtml}
      <div class="message-row ${isMe ? 'me' : 'other'} ${clusterClass}" id="msg-${m.id || ''}">
        ${avatarHtml}
        <div class="message-content-wrapper">
          <div class="message-bubble-container">
            ${isMe ? `${actionsHtml}${bubbleHtml}` : `${bubbleHtml}${actionsHtml}`}
          </div>
          ${timeHtml}
        </div>
      </div>
    `;
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
    const isShown = picker.classList.contains('show');
    this.closeAllMessageDropdowns();
    if (!isShown) {
      picker.classList.add('show');
      const row = picker.closest('.message-row');
      if (row) row.classList.add('active-dropdown');

      const chatBody = picker.closest('.chat-body');
      if (chatBody) {
        const bodyRect = chatBody.getBoundingClientRect();
        const btn = picker.parentElement;
        const btnRect = btn.getBoundingClientRect();
        if (btnRect.top - bodyRect.top < 100) {
          picker.classList.add('open-down');
        } else {
          picker.classList.remove('open-down');
        }
      }
    }
  }

  closeAllMessageDropdowns() {
    document.querySelectorAll('.msg-more-dropdown.show, .msg-reaction-picker.show').forEach(el => {
      el.classList.remove('show');
      el.classList.remove('open-down');
    });
    document.querySelectorAll('.message-row.active-dropdown').forEach(r => {
      r.classList.remove('active-dropdown');
    });
  }

  async reactToChatMessage(msgId, reaction) {
    this.closeAllMessageDropdowns();
    const isGroup = this.currentChatPartner && this.currentChatPartner.isGroup;
    const url = isGroup ? '/api/groups/messages/react' : '/api/messages/react';
    try {
      const res = await fetch(url, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ message_id: msgId, reaction: reaction })
      });
      const data = await res.json();
      if (data.status === 'ok') {
        if (isGroup) this.loadGroupMessages(this.currentChatPartner.id);
        else this.loadInitialMessages();
      }
    } catch (e) {}
  }

  startReplyToMessage(msgId, authorName, snippet) {
    this.closeAllMessageDropdowns();
    this.replyingToMessage = { id: msgId, author: authorName, content: snippet };
    const bar = document.getElementById('chatReplyBar');
    const authorEl = document.getElementById('chatReplyAuthor');
    const snipEl = document.getElementById('chatReplySnippet');
    if (bar && authorEl && snipEl) {
      authorEl.textContent = authorName;
      snipEl.textContent = snippet.length > 32 ? snippet.substring(0, 32) + '...' : (snippet || 'Đính kèm');
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
        this.showToast('Đã xóa tin nhắn!', 'success');
        this.loadFriendsAndContacts();
      }
    } catch (e) {}
  }

  async sendChatMessage() {
    const input = document.getElementById('chatInput');
    if (!input || !this.currentChatPartner || !this.currentUser) return;
    const text = input.value.trim();
    const media = this.pendingChatMedia || { type: 'NONE', url: '' };
    if (!text && !media.url) return;

    const replyText = this.replyingToMessage ? this.replyingToMessage.content : '';
    const replyId = this.replyingToMessage ? this.replyingToMessage.id : 0;

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
        if (data.status === 'ok') {
          input.value = '';
          this.cancelChatReply();
          this.clearPendingChatMedia();
          this.loadGroupMessages(this.currentChatPartner.id);
        }
      } catch (e) {}
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
        if (data.status === 'ok') {
          input.value = '';
          this.cancelChatReply();
          this.stopTyping();
          this.clearPendingChatMedia();
          this.loadInitialMessages();
        }
      } catch (e) {}
    }
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
              <img src="${f.avatar || '/img/default-avatar.svg'}" style="width:32px;height:32px;border-radius:50%;object-fit:cover;">
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
        this.currentPostReactionsData = data;
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

        tabsBox.innerHTML = tabs.filter(t => t.type === 'ALL' || t.count > 0).map((t, idx) => `
          <button class="reaction-tab-btn ${idx === 0 ? 'active' : ''}" onclick="app.filterReactionsList('${t.type}', this)">
            <span>${t.label}</span>
            <span style="font-size: 0.82rem; opacity: 0.85;">${t.count}</span>
          </button>
        `).join('');

        this.renderReactionsUserList(data.users);
        const modal = document.getElementById('postReactionsModal');
        if (modal) modal.style.display = 'flex';
      }
    } catch (e) {
      console.error(e);
    }
  }

  closePostReactionsModal() {
    const modal = document.getElementById('postReactionsModal');
    if (modal) modal.style.display = 'none';
  }

  filterReactionsList(type, btn) {
    document.querySelectorAll('.reaction-tab-btn').forEach(b => b.classList.remove('active'));
    if (btn) btn.classList.add('active');

    if (!this.currentPostReactionsData || !this.currentPostReactionsData.users) return;
    const filtered = (type === 'ALL') ? this.currentPostReactionsData.users : this.currentPostReactionsData.users.filter(u => u.type === type);
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
        actionBtn = `<button class="btn-primary" style="font-size: 0.82rem; padding: 4px 10px;" onclick="app.sendFriendRequest('${u.user_id}', this)">👤+ Thêm bạn bè</button>`;
      }

      return `
        <div class="reaction-user-row">
          <div class="reaction-user-left" onclick="app.openUserProfile('${u.user_token || u.user_id}')">
            <div class="reaction-avatar-box">
              <img src="${u.avatar || '/img/default-avatar.svg'}" alt="Avatar">
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
    ['tabAdmReportsBtn', 'tabAdmUsersBtn', 'tabAdmPostsBtn'].forEach(id => {
      document.getElementById(id)?.classList.remove('active');
    });
    ['admSectionReports', 'admSectionUsers', 'admSectionPosts'].forEach(id => {
      const el = document.getElementById(id);
      if (el) el.style.display = 'none';
    });

    if (tab === 'reports') {
      document.getElementById('tabAdmReportsBtn')?.classList.add('active');
      document.getElementById('admSectionReports').style.display = 'block';
      this.loadAdminReportsTable();
    } else if (tab === 'users') {
      document.getElementById('tabAdmUsersBtn')?.classList.add('active');
      document.getElementById('admSectionUsers').style.display = 'block';
      this.loadAdminUsersTable();
    } else if (tab === 'posts') {
      document.getElementById('tabAdmPostsBtn')?.classList.add('active');
      document.getElementById('admSectionPosts').style.display = 'block';
      this.loadAdminPostsTable();
    }
  }

  async loadAdminStats() {
    try {
      const res = await fetch('/api/admin/stats');
      const data = await res.json();
      if (data.status === 'ok') {
        const s = data.stats;
        document.getElementById('admStatUsers').textContent = s.total_users;
        document.getElementById('admStatLocked').textContent = s.locked_users;
        document.getElementById('admStatPosts').textContent = s.total_posts;
        document.getElementById('admStatMessages').textContent = s.total_messages;
        document.getElementById('admStatPendingReports').textContent = s.pending_reports;
        document.getElementById('admStatGroups').textContent = s.total_groups;
      }
    } catch (e) {}
  }

  async loadAdminReportsTable() {
    const tbody = document.getElementById('adminReportsTableBody');
    if (!tbody) return;

    try {
      const res = await fetch('/api/admin/reports');
      const data = await res.json();
      if (data.status === 'ok') {
        this.adminReportsCache = data.reports || [];
        this.renderAdminReportsRows(this.adminReportsCache);
      }
    } catch (e) {}
  }

  filterAdminReportsTable(keyword) {
    if (!this.adminReportsCache) return;
    const q = keyword.toLowerCase().trim();
    if (!q) {
      this.renderAdminReportsRows(this.adminReportsCache);
      return;
    }
    const filtered = this.adminReportsCache.filter(r =>
      (r.reporter_name && r.reporter_name.toLowerCase().includes(q)) ||
      (r.reason && r.reason.toLowerCase().includes(q)) ||
      (r.status && r.status.toLowerCase().includes(q)) ||
      (r.reported_user_name && r.reported_user_name.toLowerCase().includes(q)) ||
      (r.reported_user_id && r.reported_user_id.toLowerCase().includes(q)) ||
      (r.reported_post_id && r.reported_post_id.toLowerCase().includes(q))
    );
    this.renderAdminReportsRows(filtered);
  }

  renderAdminReportsRows(reports) {
    const tbody = document.getElementById('adminReportsTableBody');
    if (!tbody) return;

    if (!reports || reports.length === 0) {
      tbody.innerHTML = '<tr><td colspan="7" style="text-align: center; color: var(--text-muted); padding: 1.5rem;">Không tìm thấy báo cáo vi phạm phù hợp.</td></tr>';
      return;
    }

    tbody.innerHTML = reports.map(r => {
      let targetText = '--';
      if (r.reported_user_id) targetText = `👤 ${this.escapeHtml(r.reported_user_name)} (ID: ${r.reported_user_id})`;
      else if (r.reported_post_id) targetText = `📝 Bài viết: ${r.reported_post_id}`;
      else if (r.reported_comment_id) targetText = `💬 Bình luận: ${r.reported_comment_id}`;

      let statusBadge = `<span class="status-badge ${r.status.toLowerCase()}">${r.status}</span>`;

      return `
        <tr>
          <td>#${r.id}</td>
          <td><strong>${this.escapeHtml(r.reporter_name)}</strong></td>
          <td>${targetText}</td>
          <td>${this.escapeHtml(r.reason)}</td>
          <td>${this.formatTimeAgo(r.created_at)}</td>
          <td>${statusBadge}</td>
          <td>
            <div style="display: flex; gap: 0.35rem;">
              ${r.reported_user_id ? `<button class="btn-secondary" style="font-size: 0.75rem; color: var(--accent-red);" onclick="app.adminToggleLock('${r.reported_user_id}')">Khóa user</button>` : ''}
              ${r.reported_post_id ? `<button class="btn-secondary" style="font-size: 0.75rem; color: var(--accent-red);" onclick="app.adminDeletePost('${r.reported_post_id}')">Xóa bài</button>` : ''}
              <button class="btn-primary" style="font-size: 0.75rem;" onclick="app.adminResolveReport(${r.id}, 'RESOLVED')">Xử lý</button>
              <button class="btn-secondary" style="font-size: 0.75rem;" onclick="app.adminResolveReport(${r.id}, 'DISMISSED')">Bỏ qua</button>
            </div>
          </td>
        </tr>
      `;
    }).join('');
  }

  async loadAdminUsersTable() {
    const tbody = document.getElementById('adminUsersTableBody');
    if (!tbody) return;

    try {
      const res = await fetch('/api/admin/users');
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
        `<button class="btn-primary" style="font-size: 0.75rem; padding: 0.25rem 0.6rem;" onclick="app.adminToggleLock('${u.id}')">🔓 Mở khóa</button>` :
        `<button class="btn-secondary" style="font-size: 0.75rem; padding: 0.25rem 0.6rem; color: var(--accent-red);" onclick="app.adminToggleLock('${u.id}')">🔒 Khóa tài khoản</button>`;

      return `
        <tr>
          <td style="display: flex; align-items: center; gap: 0.5rem;">
            <img src="${u.avatar || '/img/default-avatar.svg'}" style="width: 28px; height: 28px; border-radius: 50%;">
            <strong>${this.escapeHtml(u.fullname)}</strong>
          </td>
          <td>@${u.username}<br><span style="font-size: 0.75rem; color: var(--text-muted);">${u.email}</span></td>
          <td>${this.escapeHtml(u.school || u.hometown || '--')}</td>
          <td><span style="font-weight: 700;">${u.role}</span></td>
          <td>${statusBadge}</td>
          <td>${this.formatTimeAgo(u.last_active_at)}</td>
          <td>${u.role !== 'ADMIN' ? actionBtn : '<span style="color: var(--text-muted); font-size: 0.75rem;">Quản trị</span>'}</td>
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
      const res = await fetch(`/api/posts?limit=100`);
      const data = await res.json();
      if (data.status === 'ok') {
        this.adminPostsCache = data.posts || [];
        this.renderAdminPostsRows(this.adminPostsCache);
      }
    } catch (e) {}
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
        <td><code>${p.id}</code></td>
        <td><strong>${this.escapeHtml(p.author_name)}</strong></td>
        <td style="max-width: 300px; white-space: nowrap; overflow: hidden; text-overflow: ellipsis;">
          ${p.title ? `<strong>${this.escapeHtml(p.title)}:</strong> ` : ''}${this.escapeHtml(p.content)}
        </td>
        <td>${p.media_type}</td>
        <td>${this.formatTimeAgo(p.created_at)}</td>
        <td>
          <button class="btn-secondary" style="font-size: 0.75rem; color: var(--accent-red);" onclick="app.adminDeletePost('${p.id}')">🗑️ Xóa bài</button>
        </td>
      </tr>
    `).join('');
  }

  async adminToggleLock(userId) {
    try {
      const res = await fetch('/api/admin/toggle-lock', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ user_id: userId })
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

  async adminResolveReport(reportId, action) {
    try {
      const res = await fetch('/api/admin/resolve-report', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ report_id: reportId, action: action })
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
      const res = await fetch('/api/admin/delete-post', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ post_id: postId })
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
  // 16B. MODAL CHỌN SỞ THÍCH (GIỚI HẠN 3, TÌM KIẾM, DẤU ✓ / ✕)
  // =========================================================================
  openHobbyPickerModal(targetInputId) {
    this.hobbyTargetInputId = targetInputId;
    const inputEl = document.getElementById(targetInputId);
    this.selectedHobbies = [];
    if (inputEl && inputEl.value) {
      this.selectedHobbies = inputEl.value
        .split(/[;,]/)
        .map(s => s.trim())
        .filter(s => s.length > 0)
        .slice(0, 3);
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
      if (this.selectedHobbies.length >= 3) {
        this.showToast('Giới hạn sở thích được chọn', 'error');
        return;
      }
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
    const list = document.getElementById('googleAccountsList');
    if (list) {
      const sampleAccounts = [
        { email: 'gnahk7754@gmail.com', name: 'Clone', initial: 'C', color: '#137333' },
        { email: 'khoanm@gmail.com', name: 'Nguyễn Minh Khoa', initial: 'K', color: '#1a73e8' },
        { email: 'lanph@gmail.com', name: 'Phạm Hương Lan', initial: 'L', color: '#c5221f' }
      ];
      list.innerHTML = sampleAccounts.map(acc => `
        <div class="google-account-row" onclick="app.selectGoogleAccount('${acc.email}', '${acc.name}', '')" style="display: flex; align-items: center; gap: 1rem; padding: 0.9rem 1rem; border-radius: 8px; cursor: pointer; transition: background 0.15s; border-bottom: 1px solid #282a2c;">
          <div style="width: 36px; height: 36px; border-radius: 50%; background: ${acc.color}; color: #ffffff; display: flex; align-items: center; justify-content: center; font-weight: 500; font-size: 1.1rem; flex-shrink: 0;">
            ${acc.initial}
          </div>
          <div>
            <div style="font-weight: 500; font-size: 0.95rem; color: #ffffff;">${acc.name}</div>
            <div style="font-size: 0.82rem; color: #9aa0a6;">${acc.email}</div>
          </div>
        </div>
      `).join('');
    }
    const modal = document.getElementById('googleAuthModal');
    if (modal) modal.style.display = 'flex';
  }

  promptCustomGoogleAccount() {
    const email = prompt('Nhập địa chỉ email Google của bạn:', 'gnahk7754@gmail.com');
    if (email && email.includes('@')) {
      const name = email.split('@')[0];
      this.selectGoogleAccount(email, name, '');
    }
  }

  triggerRealGoogleSignIn() {
    if (window.google && window.google.accounts && window.google.accounts.id) {
      let clientId = localStorage.getItem('cs_google_client_id') || '';
      if (!clientId) {
        const inputId = prompt(
          'Để kết nối trực tiếp với Google, vui lòng nhập Google OAuth 2.0 Client ID (từ Google Cloud Console):\n\n(Nếu chưa có, bạn có thể nhấn Hủy để sử dụng tính năng chọn tài khoản Google bên dưới)',
          '1042398472912-example.apps.googleusercontent.com'
        );
        if (inputId && inputId.trim()) {
          clientId = inputId.trim();
          localStorage.setItem('cs_google_client_id', clientId);
        } else {
          return;
        }
      }
      try {
        window.google.accounts.id.initialize({
          client_id: clientId,
          callback: (resp) => this.handleGoogleCredentialResponse(resp)
        });
        window.google.accounts.id.prompt((notification) => {
          if (notification.isNotDisplayed() || notification.isSkippedMoment()) {
            this.showToast('Google One Tap không thể hiển thị, hãy thử lại hoặc dùng tài khoản bên dưới!', 'info');
          }
        });
      } catch (e) {
        this.showToast('Lỗi Google Sign-In: ' + e.message, 'error');
      }
    } else {
      this.showToast('Đang kết nối tới dịch vụ Google Identity, vui lòng thử lại sau giây lát!', 'info');
    }
  }

  handleGoogleCredentialResponse(response) {
    try {
      const base64Url = response.credential.split('.')[1];
      const base64 = base64Url.replace(/-/g, '+').replace(/_/g, '/');
      const jsonPayload = decodeURIComponent(atob(base64).split('').map(c => {
        return '%' + ('00' + c.charCodeAt(0).toString(16)).slice(-2);
      }).join(''));
      const payload = JSON.parse(jsonPayload);
      this.selectGoogleAccount(payload.email, payload.name, payload.picture || '');
    } catch (e) {
      this.showToast('Lỗi đọc dữ liệu Google: ' + e.message, 'error');
    }
  }

  closeGoogleAuthModal() {
    const modal = document.getElementById('googleAuthModal');
    if (modal) modal.style.display = 'none';
  }

  submitCustomGoogleAccount() {
    const input = document.getElementById('customGoogleEmail');
    const email = input ? input.value.trim() : '';
    if (!email) {
      this.showToast('Vui lòng nhập địa chỉ Gmail!', 'error');
      return;
    }
    const name = email.split('@')[0];
    this.selectGoogleAccount(email, name, '/img/default-avatar.svg');
  }

  async selectGoogleAccount(email, fullname, avatar) {
    try {
      const res = await fetch('/api/auth/google-login', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ email: email, fullname: fullname, avatar: avatar })
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
    } catch (err) {
      this.showToast('Lỗi: ' + err.message, 'error');
    }
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

    toast.innerHTML = `<span>${icon}</span><span>${message}</span>`;
    container.appendChild(toast);

    setTimeout(() => {
      toast.style.opacity = '0';
      toast.style.transform = 'scale(0.85)';
      toast.style.transition = 'all 0.3s ease';
      setTimeout(() => toast.remove(), 300);
    }, 2500);
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
            ${message}
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
        body: JSON.stringify({ my_id: this.currentUser.id, target_id: targetId })
      });
      const data = await res.json();
      if (data.status === 'ok') {
        this.showToast('Đã xóa kết bạn thành công!', 'success');
        const btn = document.getElementById('profFriendStatusBtn');
        if (btn) {
          btn.outerHTML = `<button class="btn-primary" id="profActionFriendBtn" onclick="app.sendFriendRequest('${targetId}')">+ Thêm bạn bè</button>`;
        }
        const countEl = document.getElementById('profFriendCount');
        if (countEl) {
          let cur = parseInt(countEl.textContent || '0', 10);
          if (cur > 0) countEl.textContent = cur - 1;
        }
        this.loadUserFriends(targetId);
        this.loadFriendsAndContacts();
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
      const profNameEl = document.getElementById('profFullname');
      if (sugHeading && sugHeading.textContent.trim()) {
        displayName = sugHeading.textContent.trim();
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
        // Cập nhật nút trong trang cá nhân (nếu đang xem)
        const btn = document.getElementById('profFriendStatusBtn') || document.getElementById('profActionFriendBtn');
        if (btn) {
          btn.outerHTML = `<button class="btn-primary" id="profActionFriendBtn" onclick="app.sendFriendRequest('${targetId}', '${this.escapeHtml(displayName)}')">+ Thêm bạn bè</button>`;
        }
        // Cập nhật nút trong suggestion box ngay lập tức (Hình 3)
        const sugBox = document.getElementById(`sug-btn-box-${targetId}`);
        if (sugBox) {
          sugBox.innerHTML = `<button class="btn-add-friend" onclick="app.sendFriendRequest('${targetId}', '${this.escapeHtml(displayName)}')">+ Kết bạn</button>`;
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
  // READ RECEIPT — Hiển thị avatar đối phương khi họ đã đọc tin cuối
  // =========================================================================
  updateReadReceipt() {
    if (!this.currentChatPartner || this.currentChatPartner.isGroup) return;
    // Xóa các read-receipt cũ
    document.querySelectorAll('.read-receipt-avatar').forEach(e => e.remove());
    const listEl = document.getElementById('messagesList');
    if (!listEl) return;
    // Tìm tin nhắn cuối cùng của mình
    const myRows = [...listEl.querySelectorAll('.message-row.me')];
    if (myRows.length === 0) return;
    const lastMyRow = myRows[myRows.length - 1];
    const wrapper = lastMyRow.querySelector('.message-content-wrapper');
    if (!wrapper) return;
    // Tạo read receipt avatar
    const partnerAvatar = this.currentChatPartner.avatar || '/img/default-avatar.svg';
    const rcEl = document.createElement('div');
    rcEl.className = 'read-receipt-avatar';
    rcEl.innerHTML = `<img src="${partnerAvatar}" title="${this.escapeHtml(this.currentChatPartner.name)} đã xem" alt="Đã xem">`;
    // Chỉ hiện nếu đã được đọc (kiểm tra is_read trên tin cuối)
    rcEl.style.display = 'none';
    wrapper.appendChild(rcEl);
    // Poll trạng thái đã đọc
    this.checkLastMessageRead(lastMyRow.id, rcEl);
  }

  async checkLastMessageRead(rowId, rcEl) {
    if (!this.currentChatPartner || !this.currentUser) return;
    try {
      const res = await fetch(`/api/messages/read-status?my_id=${this.currentUser.id}&partner_id=${this.currentChatPartner.id}`);
      const data = await res.json();
      if (data.status === 'ok' && data.is_read) {
        rcEl.style.display = 'flex';
      }
    } catch(e) {}
  }

  startReadReceiptPoll() {
    if (this.msgReadPollInterval) clearInterval(this.msgReadPollInterval);
    if (!this.currentChatPartner || this.currentChatPartner.isGroup) return;
    this.msgReadPollInterval = setInterval(() => this.updateReadReceipt(), 5000);
  }

  stopReadReceiptPoll() {
    if (this.msgReadPollInterval) {
      clearInterval(this.msgReadPollInterval);
      this.msgReadPollInterval = null;
    }
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
}

document.addEventListener('DOMContentLoaded', () => {
  window.app = new AppClient();
});

