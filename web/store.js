// DeskBuddy Store - Cart Logic
(function(){
var cart = JSON.parse(localStorage.getItem("deskbuddy_cart") || "[]");

function saveCart() {
  localStorage.setItem("deskbuddy_cart", JSON.stringify(cart));
  renderCart();
}

window.addToCart = function(el) {
  var slug = el.getAttribute("data-slug");
  var name = el.getAttribute("data-name");
  var price = parseInt(el.getAttribute("data-price"));
  var existing = cart.findIndex(function(c) { return c.slug === slug; });
  if (existing >= 0) {
    cart[existing].qty++;
  } else {
    cart.push({ slug: slug, name: name, price: price, qty: 1 });
  }
  saveCart();
  showToast(name + " added!");
  toggleCart();
};

window.removeFromCart = function(slug) {
  cart = cart.filter(function(c) { return c.slug !== slug; });
  saveCart();
};

window.toggleCart = function() {
  document.getElementById("cartPanel").classList.toggle("open");
  document.getElementById("cartOverlay").classList.toggle("open");
};

window.closeCart = function() {
  document.getElementById("cartPanel").classList.remove("open");
  document.getElementById("cartOverlay").classList.remove("open");
};

function renderCart() {
  var badge = document.getElementById("cartBadge");
  var total = cart.reduce(function(s, i) { return s + i.qty; }, 0);
  badge.textContent = total;
  badge.style.display = total > 0 ? "inline" : "none";

  var el = document.getElementById("cartItems");
  if (cart.length === 0) {
    el.innerHTML = '<p style="color:#64748b;font-size:0.85rem">Your cart is empty.</p>';
  } else {
    el.innerHTML = cart.map(function(c) {
      return '<div class="cart-item"><div><strong>' + c.name + '</strong> x' + c.qty + '<br><span style="color:#64748b;font-size:0.75rem">$' + (c.price * c.qty / 100).toFixed(2) + '</span></div><button onclick="removeFromCart(\'' + c.slug.replace(/'/g,"\\'") + '\')">&times;</button></div>';
    }).join("");
  }

  var totalPrice = cart.reduce(function(s, i) { return s + i.price * i.qty; }, 0);
  document.getElementById("cartTotal").textContent = cart.length ? "Total: $" + (totalPrice / 100).toFixed(2) : "";
  document.getElementById("checkoutBtn").disabled = cart.length === 0;
}

window.checkout = async function() {
  var email = document.getElementById("checkout-email").value.trim();
  var items = cart.map(function(c) { return { slug: c.slug, qty: c.qty }; });
  var btn = document.getElementById("checkoutBtn");
  btn.disabled = true;
  btn.textContent = "Redirecting...";
  try {
    var r = await fetch("/api/checkout", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ items: items, email: email || undefined })
    });
    var d = await r.json();
    if (d.url) { window.location.href = d.url; return; }
    showToast(d.error || "Checkout failed");
    btn.disabled = false;
    btn.textContent = "Checkout";
  } catch (e) {
    showToast("Network error");
    btn.disabled = false;
    btn.textContent = "Checkout";
  }
};

function showToast(msg) {
  var t = document.getElementById("toast");
  t.textContent = msg;
  t.classList.add("show");
  setTimeout(function() { t.classList.remove("show"); }, 2500);
}

// Expose to <button onclick="">
window.showToast = showToast;

renderCart();
})();
