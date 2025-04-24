-- This file is released under the terms of the Artistic License.  Please see
-- the file LICENSE, included in this package, for details.
--
-- Copyright The DBT-5 Authors
--
-- Based on TPC-E Standard Specification Revision 1.14.0

-- Clause 3.3.7.3

-- 获取客户信息: 使用传入的账户 ID 获取客户、客户账户和账户经纪人的信息
CREATE OR REPLACE FUNCTION TradeOrderFrame1 ( -- MARK: TradeOrderFrame1
    IN acct_id IDENT_T           -- 输入: 账户ID
  , OUT acct_name VARCHAR(50)    -- 输出: 账户名称
  , OUT broker_id IDENT_T        -- 输出: 经纪人ID
  , OUT broker_name VARCHAR(100) -- 输出: 经纪人名称
  , OUT cust_f_name VARCHAR(30)  -- 输出: 客户名
  , OUT cust_id IDENT_T          -- 输出: 客户ID
  , OUT cust_l_name VARCHAR(30)  -- 输出: 客户姓
  , OUT cust_tier SMALLINT       -- 输出: 客户等级
  , OUT num_found INTEGER        -- 输出: 找到的记录个数
  , OUT tax_id VARCHAR(20)       -- 输出: 税号
  , OUT tax_status SMALLINT      -- 输出: 税务状态
) RETURNS RECORD                 -- 返回: RECORD 类型
AS $$
DECLARE
    -- variables
    rs RECORD;
BEGIN
    -- Get account, customer, and broker information
    SELECT ca_name
         , ca_b_id
         , ca_c_id
         , ca_tax_st
    -- 将查询结果存储到相应的输出参数中
    INTO acct_name
       , broker_id
       , cust_id
       , tax_status
    FROM customer_account
    WHERE ca_id = acct_id;
    -- 获取上一条查询影响的行数, 存储到 num_found 中
    GET DIAGNOSTICS num_found = ROW_COUNT;
    SELECT c_f_name
         , c_l_name
         , c_tier
         , c_tax_id
    INTO cust_f_name
       , cust_l_name
       , cust_tier
       , tax_id
    FROM customer
    WHERE c_id = cust_id;
    SELECT b_name
    INTO broker_name
    FROM broker
    WHERE b_id = broker_id;
END;
$$
LANGUAGE 'plpgsql';

-- Clause 3.3.7.4

-- 验证执行者: 验证执行交易的人是否具有适当的授权.
-- 如果执行者未获授权, 事务将回滚(CE 总是生成授权的执行者)
CREATE OR REPLACE FUNCTION TradeOrderFrame2 ( -- MARK: TradeOrderFrame2
    IN acct_id IDENT_T         -- 输入: 账户ID
  , IN exec_f_name VARCHAR(30) -- 输入: 执行者名
  , IN exec_l_name VARCHAR(30) -- 输入: 执行者姓
  , IN exec_tax_id VARCHAR(20) -- 输入: 执行者税号
  , OUT ap_acl VARCHAR(4)      -- 输出: 账户权限
) RETURNS VARCHAR(4)           -- 返回: VARCHAR(4) 类型
AS $$
BEGIN
    SELECT account_permission.ap_acl
    INTO TradeOrderFrame2.ap_acl
    FROM account_permission
    WHERE ap_ca_id = acct_id
      AND ap_f_name = exec_f_name
      AND ap_l_name = exec_l_name
      AND ap_tax_id = exec_tax_id;
END;
$$
LANGUAGE 'plpgsql';

-- Clause 3.3.7.5

-- 估算交易影响: 
--   对于限价单，使用请求的价格进行估算；
--   对于市价单，使用当前市场价值。
-- 估算过程包括评估交易对现有持仓的影响，计算可能实现的利润的资本收益税，以及计算行政费用和经纪人佣金.
-- 如果是保证金交易，还会评估客户账户的总资产
CREATE OR REPLACE FUNCTION TradeOrderFrame3 ( -- MARK: TradeOrderFrame3
    IN acct_id IDENT_T              -- 输入: 账户ID
  , IN cust_id IDENT_T              -- 输入: 客户ID
  , IN cust_tier SMALLINT           -- 输入: 客户等级
  , IN is_lifo SMALLINT             -- 输入: 是否后进先出
  , IN issue CHAR(6)                -- 输入: 证券发行
  , IN st_pending_id CHAR(4)        -- 输入: 待处理状态ID
  , IN st_submitted_id CHAR(4)      -- 输入: 已提交状态ID
  , IN tax_status SMALLINT          -- 输入: 税务状态
  , IN trade_qty S_QTY_T            -- 输入: 交易数量
  , IN trade_type_id CHAR(3)        -- 输入: 交易类型ID
  , IN type_is_margin SMALLINT      -- 输入: 是否保证金交易
  , INOUT co_name VARCHAR(60)       -- 输入/输出: 公司名称
  , INOUT requested_price S_PRICE_T -- 输入/输出: 请求价格
  , INOUT symbol VARCHAR(15)        -- 输入/输出: 证券符号
  , OUT buy_value BALANCE_T         -- 输出: 买入价值
  , OUT charge_amount VALUE_T       -- 输出: 收费金额
  , OUT comm_rate S_PRICE_T         -- 输出: 佣金费率
  , OUT acct_assets VALUE_T         -- 输出: 账户资产
  , OUT market_price S_PRICE_T      -- 输出: 市场价格
  , OUT s_name VARCHAR(70)          -- 输出: 证券名称
  , OUT sell_value BALANCE_T        -- 输出: 卖出价值
  , OUT status_id CHAR(4)           -- 输出: 状态ID
  , OUT tax_amount VALUE_T          -- 输出: 税额
  , OUT type_is_market SMALLINT     -- 输出: 是否市场交易
  , OUT type_is_sell SMALLINT       -- 输出: 是否卖出交易
) RETURNS RECORD                    -- 返回: RECORD 类型
AS $$
<< tof3 >>  -- 标记一个位置, 用于控制流程(类似于 goto 语句)
DECLARE
    -- variables
    co_id IDENT_T;
    exch_id CHAR(6);
    tax_rates S_PRICE_T;
    acct_bal BALANCE_T;
    hold_assets S_PRICE_T;
    -- Local frame variables used when estimating impact of this trade on
    -- any current holdings of the same security.
    hold_price S_PRICE_T;
    hold_qty S_QTY_T;
    needed_qty S_QTY_T;
    hs_qty S_QTY_T;
    -- cursor
    hold_list REFCURSOR;
    tmp_type_is_market BOOLEAN;
    tmp_type_is_sell BOOLEAN;
BEGIN
    hs_qty := 0;
    comm_rate := 0;
    hold_assets := NULL;
    -- Get information on the security
    IF symbol = '' THEN -- 如果证券符号为空,
        -- 则根据公司名称查询公司ID,
        SELECT company.co_id
        INTO co_id
        FROM company
        WHERE company.co_name = TradeOrderFrame3.co_name;
        -- 并根据公司ID和发行信息查询交易所ID、证券名称和证券符号
        SELECT s_ex_id
             , security.s_name
             , s_symb
        INTO exch_id
           , s_name
           , symbol
        FROM security
        WHERE s_co_id = co_id
          AND s_issue = issue;
    ELSE -- 如果证券符号不为空,
        -- 则根据证券符号查询公司ID, 交易所ID和证券名称,
        SELECT s_co_id
             , s_ex_id
             , security.s_name
        INTO co_id
           , exch_id
           , TradeOrderFrame3.s_name
        FROM security
        WHERE s_symb = symbol;
        -- 并根据公司ID查询公司名称
        SELECT company.co_name
        INTO TradeOrderFrame3.co_name
        FROM company
        WHERE company.co_id = tof3.co_id;
    END IF;
    -- Get current pricing information for the security
    SELECT lt_price
    INTO market_price
    FROM last_trade
    WHERE lt_s_symb = symbol;
    -- Set trade characteristics based on the type of trade.
    SELECT tt_is_mrkt
         , tt_is_sell
    INTO tmp_type_is_market
       , tmp_type_is_sell
    FROM trade_type
    WHERE tt_id = trade_type_id;
    IF tmp_type_is_market THEN
        type_is_market := 1;
    ELSE
        type_is_market := 0;
    END IF;
    IF tmp_type_is_sell THEN
        type_is_sell := 1;
    ELSE
        type_is_sell := 0;
    END IF;
    -- If this is a limit-order, then the requested_price was passed in to the
    -- frame, but if this a market-order, then the requested_price needs to be
    -- set to the current market price.
    IF tmp_type_is_market THEN
        requested_price = market_price;
    END IF;
    -- Initialize variables
    buy_value = 0.0;
    sell_value = 0.0;
    needed_qty = trade_qty;
    -- No prior holdings exist – if no rows returned.
    SELECT coalesce(holding_summary.hs_qty , 0)
    INTO hs_qty
    FROM holding_summary
    WHERE hs_ca_id = acct_id
      AND hs_s_symb = symbol;
    IF tmp_type_is_sell THEN
        -- This is a sell transaction, so estimate the impact to any currently held
        -- long positions in the security.
        IF hs_qty > 0 THEN
            IF is_lifo = 1 THEN
                -- Estimates will be based on closing most recently acquired
                -- holdings
                -- Could return 0, 1 or many rows
                OPEN hold_list FOR -- 打开游标
                    SELECT h_qty
                         , h_price
                    FROM holding
                    WHERE h_ca_id = acct_id
                      AND h_s_symb = symbol
                    ORDER BY h_dts DESC;
            ELSE
                -- Estimates will be based on closing oldest holdings
                -- Could return 0, 1 or many rows
                OPEN hold_list FOR -- 打开游标
                    SELECT h_qty
                         , h_price
                    FROM holding
                    WHERE h_ca_id = acct_id
                      AND h_s_symb = symbol
                    ORDER BY h_dts ASC;
            END IF;
            -- Estimate, based on the requested price, any profit that may be
            -- realized by selling current holdings for this security. The
            -- customer may have multiple holdings for this security
            -- (representing different purchases of this security at different
            -- times and therefore, most likely, different prices).
            WHILE needed_qty > 0 LOOP
                FETCH hold_list
                INTO hold_qty
                   , hold_price;
                EXIT WHEN NOT FOUND;
                IF hold_qty > needed_qty THEN -- 当前持仓数量大于需要卖出的数量
                    -- Only a portion of this holding would be sold as a result
                    -- of the trade.
                    buy_value = buy_value + (needed_qty * hold_price);
                    sell_value = sell_value + (needed_qty * requested_price);
                    needed_qty = 0;
                ELSE -- 当前持仓数量小于等于需要卖出的数量
                    -- All of this holding would be sold as a result of this
                    -- trade.
                    buy_value = buy_value + (hold_qty * hold_price);
                    sell_value = sell_value + (hold_qty * requested_price);
                    needed_qty = needed_qty - hold_qty;
                END IF;
            END LOOP;
            CLOSE hold_list;
        END IF;
        -- NOTE: If needed_qty is still greater than 0 at this point, then the
        -- customer would be liquidating all current holdings for this
        -- security, and then short-selling this remaining balance for the
        -- transaction.
    ELSE
        -- This is a buy transaction, so estimate the impact to any currently
        -- held short positions in the security. These are represented as
        -- negative H_QTY holdings. Short postions will be covered before
        -- opening a long postion in this security.
        IF hs_qty < 0 THEN
            -- Existing short position to buy
            -- 根据是否为后进先出, 选择最近或最早的持仓
            IF is_lifo = 1 THEN
                -- Estimates will be based on closing most recently acquired
                -- holdings
                -- Could return 0, 1 or many rows
                OPEN hold_list FOR
                    SELECT h_qty
                         , h_price
                    FROM holding
                    WHERE h_ca_id = acct_id
                      AND h_s_symb = symbol
                    ORDER BY h_dts DESC;
            ELSE
                -- Estimates will be based on closing oldest holdings
                -- Could return 0, 1 or many rows
                OPEN hold_list FOR
                    SELECT h_qty
                         , h_price
                    FROM holding
                    WHERE h_ca_id = acct_id
                      AND h_s_symb = symbol
                    ORDER BY h_dts ASC;
            END IF;
            -- Estimate, based on the requested price, any profit that may be
            -- realized by covering short postions currently held for this
            -- security. The customer may have multiple holdings for this
            -- security (representing different purchases of this security at
            -- different times).
            WHILE needed_qty > 0 LOOP
                FETCH hold_list
                INTO hold_qty
                   , hold_price;
                EXIT WHEN NOT FOUND;
                IF (hold_qty + needed_qty < 0) THEN
                    -- Only a portion of this holding would be covered (bought
                    -- back) as a result of this trade.
                    sell_value = sell_value + (needed_qty * hold_price);
                    buy_value = buy_value + (needed_qty * requested_price);
                    needed_qty = 0;
                ELSE
                    -- All of this holding would be covered (bought back) as
                    -- a result of this trade.
                    -- NOTE: Local variable hold_qty is made positive for easy
                    -- calculations
                    hold_qty = - hold_qty;
                    sell_value = sell_value + (hold_qty * hold_price);
                    buy_value = buy_value + (hold_qty * requested_price);
                    needed_qty = needed_qty - hold_qty;
                END IF;
            END LOOP;
            CLOSE hold_list;
        END IF;
        -- NOTE: If needed_qty is still greater than 0 at this point, then the
        -- customer would cover all current short positions (if any) for this
        -- security, and then open a new long position for the remaining
        -- balance of this transaction.
    END IF;
    -- Estimate any capital gains tax that would be incurred as a result of this
    -- transaction.
    tax_amount = 0.0;
    IF (sell_value > buy_value) AND ((tax_status = 1) OR (tax_status = 2)) THEN
        -- Customers may be subject to more than one tax at different rates.
        -- Therefore, get the sum of the tax rates that apply to the customer
        -- and estimate the overall amount of tax that would result from this
        -- order.
        -- 查询适用于客户的税率总和
        SELECT sum(tx_rate)
        INTO tax_rates
        FROM taxrate
        WHERE tx_id IN (
                           SELECT cx_tx_id
                           FROM customer_taxrate
                           WHERE cx_c_id = cust_id
                       );
        tax_amount = (sell_value - buy_value) * tax_rates;
    END IF;
    -- Get administrative fees (e.g. trading charge, commission rate)
    -- 获取行政费用
    SELECT cr_rate
    INTO comm_rate
    FROM commission_rate
    WHERE cr_c_tier = cust_tier
      AND cr_tt_id = trade_type_id
      AND cr_ex_id = exch_id
      AND cr_from_qty <= trade_qty
      AND cr_to_qty >= trade_qty;
    SELECT ch_chrg
    INTO charge_amount
    FROM charge
    WHERE ch_c_tier = cust_tier
      AND ch_tt_id = trade_type_id;
    -- Compute assets on margin trades
    -- 计算保证金交易的资产
    acct_assets = 0.0;
    IF type_is_margin = 1 THEN
        SELECT ca_bal
        INTO acct_bal
        FROM customer_account
        WHERE ca_id = acct_id;
        -- Should return 0 or 1 row
        SELECT sum(holding_summary.hs_qty * lt_price)
        INTO hold_assets
        FROM holding_summary
           , last_trade
        WHERE hs_ca_id = acct_id
          AND lt_s_symb = hs_s_symb;
        IF hold_assets IS NULL THEN -- 当前账户是否有持仓
            -- account currently has no holdings
            acct_assets = acct_bal;
        ELSE
            acct_assets = hold_assets + acct_bal;
        END IF;
    END IF;
    -- Set the status for this trade
    IF tmp_type_is_market THEN
        status_id = st_submitted_id; -- 已提交状态
    ELSE
        status_id = st_pending_id; -- 待处理状态
    END IF;
END;
$$
LANGUAGE 'plpgsql';

-- Clause 3.3.7.6

CREATE OR REPLACE FUNCTION TradeOrderFrame4 ( -- MARK: TradeOrderFrame4
    IN acct_id IDENT_T           -- 输入: 账户ID
  , IN broker_id IDENT_T         -- 输入: 经纪人ID
  , IN charge_amount VALUE_T     -- 输入: 收费金额
  , IN comm_amount VALUE_T       -- 输入: 佣金金额
  , IN exec_name CHAR(64)        -- 输入: 执行者名
  , IN is_cash SMALLINT          -- 输入: 是否现金交易
  , IN is_lifo SMALLINT          -- 输入: 是否后进先出
  , IN requested_price S_PRICE_T -- 输入: 请求价格
  , IN status_id CHAR(4)         -- 输入: 状态ID
  , IN symbol VARCHAR(15)        -- 输入: 证券符号
  , IN trade_qty S_QTY_T         -- 输入: 交易数量
  , IN trade_type_id CHAR(3)     -- 输入: 交易类型ID
  , IN type_is_market SMALLINT   -- 输入: 是否市价单
  , OUT trade_id TRADE_T         -- 输出: 交易ID
) RETURNS TRADE_T                -- 返回: TRADE_T 类型
AS $$
DECLARE
    -- variables
    now_dts TIMESTAMP;
    tmp_is_cash BOOLEAN;
    tmp_is_lifo BOOLEAN;
BEGIN
    -- Get the timestamp
    SELECT now()
    INTO now_dts;
    IF is_cash = 1 THEN
        tmp_is_cash := TRUE;
    ELSE
        tmp_is_cash := FALSE;
    END IF;
    IF is_lifo = 1 THEN
        tmp_is_lifo := TRUE;
    ELSE
        tmp_is_lifo := FALSE;
    END IF;
    -- Record trade information in TRADE table.
    INSERT INTO trade (
        t_id
      , t_dts
      , t_st_id
      , t_tt_id
      , t_is_cash
      , t_s_symb
      , t_qty
      , t_bid_price
      , t_ca_id
      , t_exec_name
      , t_trade_price
      , t_chrg
      , t_comm
      , t_tax
      , t_lifo
    )
    VALUES (
        nextval('seq_trade_id') -- 生成下一个交易ID(类型为 sequence 的 seq_trade_id 序列)
      , now_dts
      , status_id
      , trade_type_id
      , tmp_is_cash
      , symbol
      , trade_qty
      , requested_price
      , acct_id
      , exec_name
      , NULL
      , charge_amount
      , comm_amount
      , 0 -- 税额初始化为 0
      , tmp_is_lifo
    )
    RETURNING t_id -- 返回交易ID
    INTO trade_id;
    -- Record pending trade information in TRADE_REQUEST table if this trade
    -- is a limit trade
    IF type_is_market = 0 THEN -- 如果不是市价单, 则为限价单
        INSERT INTO trade_request (
            tr_t_id
          , tr_tt_id
          , tr_s_symb
          , tr_qty
          , tr_bid_price
          , tr_b_id
        )
        VALUES (
            trade_id
          , trade_type_id
          , symbol
          , trade_qty
          , requested_price
          , broker_id
        );
    END IF;
    -- Record trade information in TRADE_HISTORY table.
    INSERT INTO trade_history (
        th_t_id
      , th_dts
      , th_st_id
    )
    VALUES (
        trade_id
      , now_dts
      , status_id
    );
END;
$$
LANGUAGE 'plpgsql';
