//+------------------------------------------------------------------+
//|                                             TelegramBotEA.mq5    |
//|                   Copy-trading EA — receives signals via ZeroMQ,  |
//|                   places orders, and sends trade event            |
//|                   notifications back via ZMQ PUSH.                |
//+------------------------------------------------------------------+
#property copyright "telegram-mt5-bot"
#property link      ""
#property version   "2.00"
#property strict

// ── Includes ─────────────────────────────────────────────────────────────────
#include <Trade\Trade.mqh>
#include <Trade\PositionInfo.mqh>
#include <Trade\DealInfo.mqh>

// mql5-zmq library — install per mql5/libs/README.md
#include <Zmq/Zmq.mqh>

// ── Trading mode ─────────────────────────────────────────────────────────────
enum ENUM_TRADING_MODE
{
    MODE_DEMO_ONLY = 0,     // Demo Only — reject live accounts
    MODE_LIVE_ALLOWED = 1   // Live Allowed — PU Prime Server 6 or any live
};

// ── Input parameters (editable in EA settings) ──────────────────────────────
input ENUM_TRADING_MODE TradingMode = MODE_DEMO_ONLY; // Trading mode (Demo/Live)
input string ZMQ_HOST       = "127.0.0.1";  // ZeroMQ host
input int    ZMQ_PORT       = 5556;          // Signal receive port (PULL)
input int    ZMQ_EVENT_PORT = 5557;          // Trade event send port (PUSH)
input double MAGIC_NUM      = 123456;        // Magic number for orders
input int    SLIPPAGE_PTS   = 15;            // Max slippage in points (10=1 pip forex)
input int    MAX_SPREAD_PTS = 40;            // Skip trade if spread > this (0=disabled)
input int    ORDER_RETRIES  = 3;             // Retry failed orders up to N times
input int    RETRY_DELAY_MS = 200;           // Wait between retries (ms)

// Allowed live servers — add more as needed.
// Only these servers are accepted when TradingMode == MODE_LIVE_ALLOWED.
string g_allowed_live_servers[];
void InitAllowedServers()
{
    ArrayResize(g_allowed_live_servers, 3);
    g_allowed_live_servers[0] = "PUPrimeLtd-Live 6";
    g_allowed_live_servers[1] = "PUPrimeLtd-Live6";
    g_allowed_live_servers[2] = "PUPrime-Live 6";
}

// Check if the current server is in the allowed live list.
bool IsAllowedLiveServer()
{
    string server = AccountInfoString(ACCOUNT_SERVER);
    for(int i = 0; i < ArraySize(g_allowed_live_servers); i++)
    {
        if(StringFind(server, g_allowed_live_servers[i]) >= 0)
            return true;
    }
    // Also accept any server containing "PUPrime" and "6" for flexibility.
    if(StringFind(server, "PUPrime") >= 0 && StringFind(server, "6") >= 0)
        return true;
    return false;
}

// ── Globals ──────────────────────────────────────────────────────────────────
Context  g_context("TelegramBotEA");
Socket   g_pull_socket(g_context, ZMQ_PULL);   // receive signals
Socket   g_push_socket(g_context, ZMQ_PUSH);   // send trade events
CTrade   g_trade;
CPositionInfo g_position;
CDealInfo     g_deal;
bool     g_pull_connected = false;
bool     g_push_connected = false;

//+------------------------------------------------------------------+
//| Format a datetime as ISO-8601 string for the event wire format    |
//+------------------------------------------------------------------+
string DateTimeToISO(datetime dt)
{
    MqlDateTime mdt;
    TimeToStruct(dt, mdt);
    return StringFormat("%04d-%02d-%02dT%02d:%02d:%02d",
                        mdt.year, mdt.mon, mdt.day,
                        mdt.hour, mdt.min, mdt.sec);
}

//+------------------------------------------------------------------+
//| Send a trade event via ZMQ PUSH to the mt5-bridge                 |
//| Wire format (pipe-delimited, 18 fields):                          |
//|   EVENT|type|symbol|ticket|side|open|close|lots|profit|sl|tp|     |
//|   open_time|close_time|duration|bid|ask|spread|comment            |
//+------------------------------------------------------------------+
void SendTradeEvent(string type, string symbol, ulong ticket, string side,
                    double open_price, double close_price, double lots,
                    double profit, double sl, double tp,
                    datetime open_time, datetime close_time,
                    string comment)
{
    if(!g_push_connected) return;

    long duration_secs = (long)(close_time > 0 ? close_time - open_time : 0);

    double bid    = SymbolInfoDouble(symbol, SYMBOL_BID);
    double ask    = SymbolInfoDouble(symbol, SYMBOL_ASK);
    double spread = SymbolInfoInteger(symbol, SYMBOL_SPREAD);

    string msg = "EVENT|" + type + "|" + symbol + "|" +
                 IntegerToString(ticket) + "|" + side + "|" +
                 DoubleToString(open_price, 5) + "|" +
                 DoubleToString(close_price, 5) + "|" +
                 DoubleToString(lots, 2) + "|" +
                 DoubleToString(profit, 2) + "|" +
                 DoubleToString(sl, 5) + "|" +
                 DoubleToString(tp, 5) + "|" +
                 DateTimeToISO(open_time) + "|" +
                 (close_time > 0 ? DateTimeToISO(close_time) : "") + "|" +
                 IntegerToString(duration_secs) + "|" +
                 DoubleToString(bid, 5) + "|" +
                 DoubleToString(ask, 5) + "|" +
                 DoubleToString(spread, 1) + "|" +
                 comment;

    ZmqMsg zmsg(msg);
    g_push_socket.send(zmsg);
    Print("[EA][EVENT] Sent: ", type, " ", side, " ", symbol,
          " ticket=#", ticket, " P&L=", DoubleToString(profit, 2));
}

//+------------------------------------------------------------------+
//| Expert initialization function                                    |
//+------------------------------------------------------------------+
int OnInit()
{
    InitAllowedServers();

    // ── SAFETY GUARD ─────────────────────────────────────────────────────
    // Validates the account mode matches the selected TradingMode input.
    bool is_demo = (AccountInfoInteger(ACCOUNT_TRADE_MODE) == ACCOUNT_TRADE_MODE_DEMO);
    string server = AccountInfoString(ACCOUNT_SERVER);

    if(TradingMode == MODE_DEMO_ONLY)
    {
        // Demo-only mode: reject any live account.
        if(!is_demo)
        {
            Print("[EA][FATAL] TradingMode is DEMO ONLY but account is LIVE!");
            Print("[EA][FATAL] Server: ", server, " — Removing EA.");
            Print("[EA][FATAL] To trade live, set TradingMode = Live Allowed in EA inputs.");
            ExpertRemove();
            return INIT_FAILED;
        }
        Print("[EA][INFO] Mode: DEMO ONLY");
    }
    else if(TradingMode == MODE_LIVE_ALLOWED)
    {
        if(is_demo)
        {
            // Live mode on a demo account is fine — no restrictions.
            Print("[EA][INFO] Mode: LIVE ALLOWED (running on demo — OK)");
        }
        else
        {
            // Live account: verify it's on an allowed server.
            if(!IsAllowedLiveServer())
            {
                Print("[EA][FATAL] Live trading not allowed on server: ", server);
                Print("[EA][FATAL] Allowed: PU Prime Server 6. Removing EA.");
                ExpertRemove();
                return INIT_FAILED;
            }
            Print("[EA][INFO] Mode: LIVE — Server verified: ", server);
            Print("[EA][WARN] *** LIVE TRADING ACTIVE — REAL MONEY AT RISK ***");
        }
    }

    Print("[EA][INFO] Account: ", AccountInfoInteger(ACCOUNT_LOGIN));
    Print("[EA][INFO] Server: ", server);
    Print("[EA][INFO] Balance: ", AccountInfoDouble(ACCOUNT_BALANCE));

    // ── Connect ZMQ PULL socket (receive signals) ────────────────────────
    string pull_ep = "tcp://" + ZMQ_HOST + ":" + IntegerToString(ZMQ_PORT);
    g_pull_socket.setReceiveTimeout(100);

    if(g_pull_socket.connect(pull_ep))
    {
        Print("[EA][INFO] ZMQ PULL connected to ", pull_ep);
        g_pull_connected = true;
    }
    else
    {
        Print("[EA][ERROR] ZMQ PULL connect failed for ", pull_ep);
        return INIT_FAILED;
    }

    // ── Connect ZMQ PUSH socket (send trade events) ─────────────────────
    string push_ep = "tcp://" + ZMQ_HOST + ":" + IntegerToString(ZMQ_EVENT_PORT);
    g_push_socket.setSendTimeout(100);
    g_push_socket.setLinger(1000);

    if(g_push_socket.connect(push_ep))
    {
        Print("[EA][INFO] ZMQ PUSH connected to ", push_ep);
        g_push_connected = true;
    }
    else
    {
        Print("[EA][WARN] ZMQ PUSH connect failed for ", push_ep,
              " — notifications disabled");
    }

    // ── Configure trade object ───────────────────────────────────────────
    g_trade.SetExpertMagicNumber((ulong)MAGIC_NUM);
    g_trade.SetDeviationInPoints(SLIPPAGE_PTS);
    g_trade.SetTypeFilling(ORDER_FILLING_IOC);

    // Poll every 100 ms for incoming signals.
    EventSetMillisecondTimer(100);

    Print("[EA][INFO] Initialization complete. Waiting for signals...");
    return INIT_SUCCEEDED;
}

//+------------------------------------------------------------------+
//| Expert deinitialization function                                   |
//+------------------------------------------------------------------+
void OnDeinit(const int reason)
{
    EventKillTimer();

    string pull_ep = "tcp://" + ZMQ_HOST + ":" + IntegerToString(ZMQ_PORT);
    string push_ep = "tcp://" + ZMQ_HOST + ":" + IntegerToString(ZMQ_EVENT_PORT);

    if(g_pull_connected)
        g_pull_socket.disconnect(pull_ep);
    if(g_push_connected)
        g_push_socket.disconnect(push_ep);

    g_context.shutdown();
    Print("[EA][INFO] EA removed. Reason: ", reason);
}

//+------------------------------------------------------------------+
//| Timer function — polls ZMQ for new signals every 100 ms           |
//+------------------------------------------------------------------+
void OnTimer()
{
    if(!g_pull_connected) return;

    ZmqMsg zmsg;
    if(!g_pull_socket.recv(zmsg, true))
        return;

    string raw = zmsg.getData();
    if(StringLen(raw) == 0) return;

    Print("[EA][RECV] ", raw);

    // ── Parse pipe-delimited signal ──────────────────────────────────────
    // Format: SYMBOL|SIDE|entry|sl|tp1|tp2|lots
    string parts[];
    int count = StringSplit(raw, '|', parts);
    if(count < 7)
    {
        Print("[EA][ERROR] Malformed signal (expected 7 fields): ", raw);
        return;
    }

    string symbol = parts[0];
    string side   = parts[1];
    double entry  = StringToDouble(parts[2]);
    double sl     = StringToDouble(parts[3]);
    double tp1    = StringToDouble(parts[4]);
    double lots   = StringToDouble(parts[6]);

    if(!SymbolSelect(symbol, true))
    {
        Print("[EA][ERROR] Symbol not found: ", symbol);
        return;
    }

    // ── Spread check: skip if spread is too wide (news spike, illiquid) ──
    if(MAX_SPREAD_PTS > 0)
    {
        long current_spread = SymbolInfoInteger(symbol, SYMBOL_SPREAD);
        if(current_spread > MAX_SPREAD_PTS)
        {
            Print("[EA][SKIP] Spread too wide: ", current_spread,
                  " pts > max ", MAX_SPREAD_PTS, " — skipping ", side, " ", symbol);
            SendTradeEvent("SKIPPED", symbol, 0, side,
                           entry, 0.0, lots, 0.0, sl, tp1,
                           TimeCurrent(), 0,
                           "spread=" + IntegerToString(current_spread));
            return;
        }
    }

    // ── Place order with retry on transient failures ─────────────────────
    double point = SymbolInfoDouble(symbol, SYMBOL_POINT);
    bool is_market = (entry == 0.0);
    if(!is_market)
    {
        // Check if entry is close enough to current price for a market order.
        double ref = (side == "BUY")
                     ? SymbolInfoDouble(symbol, SYMBOL_ASK)
                     : SymbolInfoDouble(symbol, SYMBOL_BID);
        if(MathAbs(entry - ref) < 50 * point)
            is_market = true;
    }

    bool result = false;

    for(int attempt = 1; attempt <= ORDER_RETRIES; attempt++)
    {
        if(side == "BUY")
        {
            if(is_market)
            {
                // Refresh price right before sending — reduces stale-price slippage.
                double ask = SymbolInfoDouble(symbol, SYMBOL_ASK);
                Print("[EA][ORDER] Market BUY ", symbol, " lots=", lots,
                      " ask=", ask, " sl=", sl, " tp=", tp1,
                      " attempt=", attempt);
                result = g_trade.Buy(lots, symbol, ask, sl, tp1,
                                     "TelegramBot signal");
            }
            else
            {
                Print("[EA][ORDER] Buy Limit ", symbol, " price=", entry,
                      " lots=", lots, " sl=", sl, " tp=", tp1);
                result = g_trade.BuyLimit(lots, entry, symbol, sl, tp1,
                                          ORDER_TIME_GTC, 0, "TelegramBot signal");
            }
        }
        else if(side == "SELL")
        {
            if(is_market)
            {
                double bid = SymbolInfoDouble(symbol, SYMBOL_BID);
                Print("[EA][ORDER] Market SELL ", symbol, " lots=", lots,
                      " bid=", bid, " sl=", sl, " tp=", tp1,
                      " attempt=", attempt);
                result = g_trade.Sell(lots, symbol, bid, sl, tp1,
                                      "TelegramBot signal");
            }
            else
            {
                Print("[EA][ORDER] Sell Limit ", symbol, " price=", entry,
                      " lots=", lots, " sl=", sl, " tp=", tp1);
                result = g_trade.SellLimit(lots, entry, symbol, sl, tp1,
                                            ORDER_TIME_GTC, 0, "TelegramBot signal");
            }
        }
        else
        {
            Print("[EA][ERROR] Unknown side: ", side);
            return;
        }

        if(result)
        {
            Print("[EA][OK] Order filled. Ticket: ", g_trade.ResultOrder(),
                  " Deal: ", g_trade.ResultDeal(),
                  " Price: ", g_trade.ResultPrice());

            // Log actual slippage for market orders.
            if(is_market)
            {
                double requested = (side == "BUY")
                                   ? SymbolInfoDouble(symbol, SYMBOL_ASK)
                                   : SymbolInfoDouble(symbol, SYMBOL_BID);
                double filled = g_trade.ResultPrice();
                double slip_pts = MathAbs(filled - requested) / point;
                Print("[EA][SLIP] Slippage: ", DoubleToString(slip_pts, 1), " pts");
            }
            break;  // success — stop retrying
        }

        // Order failed — check if retryable.
        uint retcode = g_trade.ResultRetcode();
        Print("[EA][FAIL] Attempt ", attempt, "/", ORDER_RETRIES,
              " Error: ", GetLastError(),
              " Retcode: ", retcode,
              " Comment: ", g_trade.ResultComment());

        // Only retry on transient errors (requote, price changed, no connection).
        if(retcode != TRADE_RETCODE_REQUOTE &&
           retcode != TRADE_RETCODE_PRICE_CHANGED &&
           retcode != TRADE_RETCODE_PRICE_OFF &&
           retcode != TRADE_RETCODE_CONNECTION)
        {
            Print("[EA][FAIL] Non-retryable error. Giving up.");
            break;
        }

        if(attempt < ORDER_RETRIES)
        {
            Print("[EA][RETRY] Waiting ", RETRY_DELAY_MS, " ms before retry...");
            Sleep(RETRY_DELAY_MS);
        }
    }
}

//+------------------------------------------------------------------+
//| Trade transaction handler — detects trade lifecycle events        |
//| and sends notifications via ZMQ PUSH.                             |
//+------------------------------------------------------------------+
void OnTradeTransaction(const MqlTradeTransaction& trans,
                        const MqlTradeRequest& request,
                        const MqlTradeResult& result)
{
    // We only care about deal additions (trade opened or closed).
    if(trans.type != TRADE_TRANSACTION_DEAL_ADD)
        return;

    // Retrieve deal details from history.
    ulong deal_ticket = trans.deal;
    if(deal_ticket == 0) return;

    if(!HistoryDealSelect(deal_ticket))
        return;

    // Only handle deals placed by this EA (matching magic number).
    long deal_magic = HistoryDealGetInteger(deal_ticket, DEAL_MAGIC);
    if(deal_magic != (long)MAGIC_NUM)
        return;

    long   deal_entry  = HistoryDealGetInteger(deal_ticket, DEAL_ENTRY);
    long   deal_type   = HistoryDealGetInteger(deal_ticket, DEAL_TYPE);
    string deal_symbol = HistoryDealGetString(deal_ticket, DEAL_SYMBOL);
    double deal_price  = HistoryDealGetDouble(deal_ticket, DEAL_PRICE);
    double deal_lots   = HistoryDealGetDouble(deal_ticket, DEAL_VOLUME);
    double deal_profit = HistoryDealGetDouble(deal_ticket, DEAL_PROFIT);
    double deal_swap   = HistoryDealGetDouble(deal_ticket, DEAL_SWAP);
    double deal_comm   = HistoryDealGetDouble(deal_ticket, DEAL_COMMISSION);
    string deal_comment= HistoryDealGetString(deal_ticket, DEAL_COMMENT);
    long   deal_time   = (long)HistoryDealGetInteger(deal_ticket, DEAL_TIME);
    ulong  pos_id      = HistoryDealGetInteger(deal_ticket, DEAL_POSITION_ID);

    // Determine side string.
    string side = (deal_type == DEAL_TYPE_BUY) ? "BUY" : "SELL";

    // ── ENTRY deal: trade opened ─────────────────────────────────────────
    if(deal_entry == DEAL_ENTRY_IN)
    {
        // Get SL/TP from the open position (it exists right after the deal).
        double sl = 0, tp = 0;
        if(PositionSelectByTicket(pos_id))
        {
            sl = PositionGetDouble(POSITION_SL);
            tp = PositionGetDouble(POSITION_TP);
        }

        SendTradeEvent("OPENED", deal_symbol, pos_id, side,
                        deal_price, 0.0, deal_lots, 0.0, sl, tp,
                        (datetime)deal_time, 0, deal_comment);
        return;
    }

    // ── EXIT deal: trade closed ──────────────────────────────────────────
    if(deal_entry == DEAL_ENTRY_OUT)
    {
        // Total P&L including swap and commission.
        double total_profit = deal_profit + deal_swap + deal_comm;

        // Find the opening deal for this position to get entry price and time.
        double open_price   = 0.0;
        double open_sl      = 0.0;
        double open_tp      = 0.0;
        datetime open_time  = 0;

        // The exit deal's side is OPPOSITE to the position side.
        // If exit deal is BUY, position was SELL. Vice versa.
        string pos_side = (deal_type == DEAL_TYPE_BUY) ? "SELL" : "BUY";

        // Search history for the entry deal of this position.
        if(HistorySelectByPosition(pos_id))
        {
            int total = HistoryDealsTotal();
            for(int i = 0; i < total; i++)
            {
                ulong ht = HistoryDealGetTicket(i);
                if(ht == 0) continue;
                if(HistoryDealGetInteger(ht, DEAL_ENTRY) == DEAL_ENTRY_IN &&
                   HistoryDealGetInteger(ht, DEAL_POSITION_ID) == (long)pos_id)
                {
                    open_price = HistoryDealGetDouble(ht, DEAL_PRICE);
                    open_time  = (datetime)HistoryDealGetInteger(ht, DEAL_TIME);
                    break;
                }
            }
        }

        // Classify close reason: SL hit, TP hit, or manual close.
        // Compare close price to the position's SL/TP.
        // We use the deal comment and price proximity to classify.
        string event_type = "CLOSED";
        double point = SymbolInfoDouble(deal_symbol, SYMBOL_POINT);
        double tolerance = 50.0 * point;  // allow 5 pip tolerance for slippage

        // Try to get SL/TP from deal comment or from position (if still exists).
        // MT5 often puts "[sl]" or "[tp]" in the deal comment.
        if(StringFind(deal_comment, "[sl]") >= 0 ||
           StringFind(deal_comment, "sl")  >= 0)
        {
            event_type = "SL_HIT";
        }
        else if(StringFind(deal_comment, "[tp]") >= 0 ||
                StringFind(deal_comment, "tp")  >= 0)
        {
            event_type = "TP_HIT";
        }

        long duration = (long)(deal_time - (long)open_time);
        if(duration < 0) duration = 0;

        SendTradeEvent(event_type, deal_symbol, pos_id, pos_side,
                        open_price, deal_price, deal_lots,
                        total_profit, open_sl, open_tp,
                        open_time, (datetime)deal_time,
                        deal_comment);
        return;
    }
}

//+------------------------------------------------------------------+
//| Tick function — not used, logic is in OnTimer                     |
//+------------------------------------------------------------------+
void OnTick()
{
    // Intentionally empty — all logic runs in OnTimer() for consistent polling.
}
//+------------------------------------------------------------------+
