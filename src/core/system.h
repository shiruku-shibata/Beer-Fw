/**
 * システム初期化・共通周期処理
 */
#pragma once

/**
 * @brief ハードウェア（M5、シリアル）を初期化する。
 */
void system_init(void);

/**
 * @brief 毎ループ呼ぶ共通処理（ボタン状態の更新など）。
 */
void system_update(void);
